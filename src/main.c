#include "hst.h"
#include "stack.h"
#include "list.h"

#define BUFFER_SIZE 2048
#define MAX_ARGS 512

/* Holds redirection info: destination fd and target filename */
typedef struct STD_RED
{
  int dest;
  char *name;
  char *mode;
} STDRED;

/* Holds the parsed result of one input line */
typedef struct PARSED_INPUT
{
  int argc;
  char *argv[MAX_ARGS];
  STDRED *red_inst; /* NULL if no redirection was found */
  /* 23.06.26 */
  int pipe;
  int cmd_index;
  char *cmds[16];
  /* 23.06.26 */
} PARSED_INPUT;

PARSED_INPUT *parse(char *buffer);
HT *init_builtin_cmd(void);
void clean(PARSED_INPUT *parsed_input);
char *type(PARSED_INPUT *parsed_input, int cmd_index);
void cd(PARSED_INPUT *parsed_input);
void pwd(void);
void historyf(PARSED_INPUT *parsed_input, ST *history);

int main(int argc, char *argv[])
{
  /* These live for the entire shell session and are shared by each command. */
  HT *builtin_cmd = init_builtin_cmd();
  if (builtin_cmd == NULL)
    exit(EXIT_FAILURE);

  ST *history = init_stack();
  if (history == NULL)
    exit(EXIT_FAILURE);

  LIST *job_list = init_list();
  if (job_list == NULL)
    exit(EXIT_FAILURE);

  setbuf(stdout, NULL);
  rl_bind_key('\t', rl_complete);

  /* Read history on startup */
  PARSED_INPUT *startup_read = malloc(sizeof(PARSED_INPUT));
  startup_read->argc = 3;
  startup_read->argv[0] = "history";
  startup_read->argv[1] = "-r";
  startup_read->argv[2] = getenv("HISTFILE");
  startup_read->argv[3] = NULL;
  historyf(startup_read, history);
  free(startup_read);

  char *buffer;
  while (true)
  {
    /* readline() allocates buffer; each normal loop path releases it. */
    buffer = readline("$ ");
    add_history(buffer);
    push(history, buffer);

    PARSED_INPUT *parsed_input = parse(buffer);
    /* The table value is the built-in's name, used for dispatch below. */
    char *cmd = ht_get(builtin_cmd, parsed_input->argv[0]);

    /* DEBUG */
    /*for (int i = 0; parsed_input->argv[i] != NULL; i++)
    {
      printf("%s ", parsed_input->argv[i]);
    }
    printf("%s\n", parsed_input->red_inst->name);
    printf("\n");*/

    /* Open redirection target if one was parsed */
    FILE *dest = NULL;
    int saved_std = -1;
    if (parsed_input->red_inst != NULL)
    {
      char *mode = (strcmp(parsed_input->red_inst->mode, "w") == 0) ? "w" : "a";
      dest = fopen(parsed_input->red_inst->name, mode);
      if (dest == NULL)
      {
        printf("Couldn't open destination file. Error number: %d\n", errno);
        clean(parsed_input);
        free(buffer);
        continue;
      }

      /* Redirect stdout to the file for built-in commands */
      if (cmd != NULL)
      {
        /* Built-ins run in this process, so temporarily replace its stdout. */
        saved_std = dup(STDOUT_FILENO);
        dup2(fileno(dest), parsed_input->red_inst->dest);
      }
    }

    /* Dispatch to matching built-in handler, or fork an external executable */
    if (cmd != NULL && strcmp(cmd, "exit") == 0)
    {
      clean(parsed_input);
      free(buffer);
      break;
    }
    else if (cmd != NULL && strcmp(cmd, "history") == 0)
    {
      historyf(parsed_input, history);
    }
    else if (cmd != NULL && strcmp(cmd, "jobs") == 0)
    {
      print_list(job_list);
    }
    else if (cmd != NULL && strcmp(cmd, "echo") == 0)
    {
      if (parsed_input->argc > 1)
      {
        for (int i = 1; parsed_input->argv[i] != NULL; i++)
          printf("%s ", parsed_input->argv[i]);
      }
      printf("\n");
    }
    else if (cmd != NULL && strcmp(cmd, "type") == 0)
    {
      if (parsed_input->argc < 2)
      {
        printf(": not found\n");
        clean(parsed_input);
        free(buffer);
        continue;
      }

      if (ht_get(builtin_cmd, parsed_input->argv[1]) != NULL)
      {
        printf("%s is a shell builtin\n", parsed_input->argv[1]);
        clean(parsed_input);
        free(buffer);
        continue;
      }

      char *path = type(parsed_input, 1);
      if (path == NULL)
      {
        printf("%s: not found\n", parsed_input->argv[1]);
        clean(parsed_input);
        free(buffer);
        continue;
      }

      printf("%s is %s\n", parsed_input->argv[1], path);
      free(path);
    }
    else if (cmd != NULL && strcmp(cmd, "cd") == 0)
      cd(parsed_input);
    else if (cmd != NULL && strcmp(cmd, "pwd") == 0)
      pwd();
    else
    {
      char *exe_path = type(parsed_input, 0);
      if (exe_path == NULL)
      {
        printf("%s: command not found\n", parsed_input->argv[0]);
        clean(parsed_input);
        free(buffer);
        continue;
      }

      bool back_job = false;
      static int job_n = 0;
      if (strcmp(parsed_input->argv[parsed_input->argc - 1], "&") == 0)
      {
        back_job = true;
        parsed_input->argv[parsed_input->argc - 1] = NULL;
        job_n++;
      }

      pid_t parent = getpid();
      pid_t child = fork();

      if (child == -1)
      {
        printf("Error number %d\n", errno);
        free(exe_path);
        clean(parsed_input);
        free(buffer);
        continue;
      }

      if (getpid() == parent)
      {
        /* The parent keeps job metadata; only foreground jobs are waited for. */
        append(job_list, job_n, child, buffer, "Running");
        if (!back_job)
        {
          wait(NULL);
        }
        sleep(1);
      }

      if (child == 0)
      {
        // printf("[DEBUG] It's me - a child process!\n");
        if (back_job)
        {
          printf("[%d] %d\n", job_n, getpid());
        }

        if (dest != NULL)
        {
          int std;
          switch (parsed_input->red_inst->dest)
          {
          case STDOUT_FILENO:
            std = STDOUT_FILENO;
            break;
          case STDERR_FILENO:
            std = STDERR_FILENO;
            break;
          case STDIN_FILENO:
            std = STDIN_FILENO;
            break;
          default:
            std = 0;
          }

          /* dup2 makes the selected standard descriptor refer to dest. */
          int is_redirected = dup2(fileno(dest), std);
          if (is_redirected == -1)
            fprintf(stderr, "Failed to redirect: %d\n", is_redirected);
        }

        /* execv replaces this child; it returns only when execution fails. */
        int status = execv(exe_path, parsed_input->argv);
        if (status == -1)
          printf("Error number %d\n", errno);
        exit(status);
      }

      free(exe_path);
      clean(parsed_input);
      free(buffer);
      continue;
    }

    /* Restore stdout and close the redirection file if one was opened */
    if (dest != NULL)
    {
      dup2(saved_std, STDOUT_FILENO);
      fclose(dest);
    }

    clean(parsed_input);
    free(buffer);
  }

  /* Write history on exit */
  PARSED_INPUT *exit_write = malloc(sizeof(PARSED_INPUT));
  exit_write->argc = 3;
  exit_write->argv[0] = "history";
  exit_write->argv[1] = "-w";
  exit_write->argv[2] = getenv("HISTFILE");
  exit_write->argv[3] = NULL;
  historyf(exit_write, history);
  free(exit_write);

  free_list(job_list);
  ht_destroy(builtin_cmd);
  exit(EXIT_SUCCESS);
}

/*
 * parse – tokenises one input line into argv tokens.
 * Handles single quotes (literal), double quotes (allow backslash),
 * backslash escapes, and output redirection (">" / "1>" / "2>").
 * Fills parsed_input->red_inst when a redirection operator is found.
 */
PARSED_INPUT *parse(char *buffer)
{
  PARSED_INPUT *parsed_input = (PARSED_INPUT *)malloc(sizeof(PARSED_INPUT));
  parsed_input->red_inst = NULL;
  /* Pipe detection is recorded here; execution is not implemented yet. */
  parsed_input->pipe = 0;
  parsed_input->cmd_index = 0;
  /* 23.06.26 */

  char parsed_token[BUFFER_SIZE];
  int token_i = 0;
  int arg_i = 0;
  bool is_quoted = false;
  bool is_dquoted = false;

  for (char *ch = buffer; *ch != '\0'; ch++)
  {
    /* Retain this marker for a future pipeline executor. */
    if (*ch == '|')
    {
      parsed_input->pipe = 1;
      continue;
    }

    /* Detect redirection operators: ">", "1>", "2>" */
    if (((*ch == '1' || *ch == '2') && *(ch + 1) == '>') || *ch == '>')
    {
      if (*(ch + 1) == '\0')
        continue;

      parsed_input->red_inst = (STDRED *)malloc(sizeof(STDRED));
      parsed_input->red_inst->name = NULL;
      parsed_input->red_inst->mode = NULL;
      parsed_input->red_inst->dest = STDOUT_FILENO;

      if (*ch != '>')
        parsed_input->red_inst->dest = atoi(ch++);

      if (*ch == '>' && *(ch + 1) == '>')
      {
        parsed_input->red_inst->mode = strdup("a");
        ch++;
        continue;
      }

      parsed_input->red_inst->mode = strdup("w");
      continue;
    }

    /* Handle quote characters */
    if (*ch == '\'' || *ch == '"')
    {
      if (*ch == '"')
      {
        if (is_dquoted)
        {
          is_dquoted = !is_dquoted;
          if (is_quoted)
            is_quoted = !is_quoted;
          continue;
        }
        if (is_quoted)
        {
          parsed_token[token_i++] = *ch;
          continue;
        }
        is_dquoted = !is_dquoted;
        continue;
      }

      /* Single quote inside double quotes: treat as literal */
      if (is_dquoted)
      {
        parsed_token[token_i++] = *ch;
        is_quoted = !is_quoted;
        continue;
      }

      is_quoted = !is_quoted;
      continue;
    }

    /* Backslash outside quotes: next character is literal */
    if (*ch == '\\' && !is_quoted && !is_dquoted)
    {
      parsed_token[token_i++] = *(++ch);
      continue;
    }

    /* Backslash inside double quotes: next character is literal */
    if (*ch == '\\' && is_dquoted)
    {
      parsed_token[token_i++] = *(++ch);
      continue;
    }

    /* Unquoted space: token delimiter */
    if (*ch == ' ' && !is_quoted && !is_dquoted)
    {
      if (token_i == 0)
        continue; /* skip leading/consecutive spaces */

      parsed_token[token_i] = '\0';
      parsed_input->argv[arg_i++] = strdup(parsed_token);
      token_i = 0;
    }
    else
      parsed_token[token_i++] = *ch;
  }

  /* Flush the last token, stripping any trailing newline */
  if (token_i > 1 && parsed_token[token_i - 1] == '\n')
    parsed_token[token_i - 1] = '\0';
  else
    parsed_token[token_i] = '\0';

  parsed_input->argv[arg_i++] = strdup(parsed_token);
  parsed_input->argc = arg_i;
  parsed_input->argv[arg_i] = NULL;

  if (parsed_input->red_inst != NULL)
  {
    /* The last argv token is the redirect filename; save and remove it */
    parsed_input->red_inst->name = strdup(parsed_input->argv[parsed_input->argc - 1]);
    /* Removing it keeps argv valid for execv and built-in handlers. */
    parsed_input->argv[parsed_input->argc - 1] = NULL;
    parsed_input->argc--;
  }

  return parsed_input;
}

/* Populate the hash table with the names of every built-in command */
HT *init_builtin_cmd(void)
{
  HT *builtin_cmd = ht_create();
  ht_set(builtin_cmd, "echo", "echo");
  ht_set(builtin_cmd, "exit", "exit");
  ht_set(builtin_cmd, "type", "type");
  ht_set(builtin_cmd, "cd", "cd");
  ht_set(builtin_cmd, "pwd", "pwd");
  ht_set(builtin_cmd, "history", "history");
  ht_set(builtin_cmd, "jobs", "jobs");
  return builtin_cmd;
}

/*
 * type – searches each directory in PATH for an executable named
 * parsed_input->argv[cmd_index]. Returns a heap-allocated full path
 * string on success, or NULL if not found or not executable.
 */
char *type(PARSED_INPUT *parsed_input, int cmd_index)
{
  char *type_arg = parsed_input->argv[cmd_index];
  if (type_arg == NULL)
    return NULL;

  char *env_path_cp = strdup(getenv("PATH"));
  /* strtok modifies its input, hence the private copy of PATH. */
  char *path = strtok(env_path_cp, ":");
  char exe_path[PATH_MAX];

  while (path != NULL)
  {
    DIR *dir = opendir(path);
    if (dir == NULL)
    {
      free(env_path_cp);
      return NULL;
    }

    /* Build the full path: "<dir>/<name>" */
    char *p = exe_path;
    memcpy(p, path, strlen(path));
    p += strlen(path);
    *p++ = '/';
    memcpy(p, type_arg, strlen(type_arg));
    p += strlen(type_arg);
    *p = '\0';

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
      if (strcmp(entry->d_name, type_arg) == 0)
      {
        struct stat st;
        stat(exe_path, &st);
        if (st.st_mode & S_IXUSR)
        {
          free(env_path_cp);
          closedir(dir);
          return strdup(exe_path);
        }
      }
    }

    closedir(dir);
    path = strtok(NULL, ":");
  }

  free(env_path_cp);
  return NULL;
}

/*
 * cd – changes the working directory.
 * Handles four prefixes:
 *   ".."  – traverse upward one level per token
 *   "."   – descend into the remainder of the relative path
 *   "~"   – go to $HOME
 *   other – treat the whole argument as an absolute path
 * Updates $PWD after a successful chdir.
 */
void cd(PARSED_INPUT *parsed_input)
{
  if (parsed_input->argc == 1)
  {
    chdir(getenv("HOME"));
    return;
  }

  char *abs_path = strdup(parsed_input->argv[1] + strcspn(parsed_input->argv[1], "/"));
  char *dest = strdup(parsed_input->argv[1] + strcspn(parsed_input->argv[1], "/") + 1);
  char *to_free = dest;
  char *token = strtok(parsed_input->argv[1], "/");
  char buffer[PATH_MAX];

  if (strcmp(token, "..") == 0)
  {
    while (token != NULL)
    {
      if (chdir("..") == -1)
      {
        free(abs_path);
        free(to_free);
        return;
      }
      token = strtok(NULL, "/");
    }
  }
  else if (strcmp(token, ".") == 0)
  {
    if (chdir(dest) == -1)
    {
      printf("cd: %s: No such file or directory\n", dest);
      free(abs_path);
      free(to_free);
      return;
    }
  }
  else if (strcmp(token, "~") == 0)
  {
    if (chdir(getenv("HOME")) == -1)
    {
      free(abs_path);
      free(to_free);
      return;
    }
  }
  else
  {
    if (chdir(abs_path) < 0)
    {
      printf("cd: %s: No such file or directory\n", abs_path);
      free(abs_path);
      free(to_free);
      return;
    }
  }

  getcwd(buffer, sizeof(buffer));
  setenv("PWD", buffer, 1);

  free(abs_path);
  free(to_free);
}

void pwd(void)
{
  printf("%s\n", getenv("PWD"));
}

/* Free all heap memory owned by a PARSED_INPUT */
void clean(PARSED_INPUT *parsed_input)
{
  if (parsed_input->red_inst != NULL)
  {
    free(parsed_input->red_inst->name);
    free(parsed_input->red_inst->mode);
    free(parsed_input->red_inst);
  }

  for (int i = 0; parsed_input->argv[i] != NULL; i++)
    free(parsed_input->argv[i]);

  free(parsed_input);
}

void historyf(PARSED_INPUT *parsed_input, ST *history)
{
  if (parsed_input->argc > 1)
  {
    if (strcmp(parsed_input->argv[1], "-r") == 0)
    {
      FILE *file = fopen(parsed_input->argv[2], "r");
      if (file == NULL)
      {
        // printf("Error number %d\n", errno);
        return;
      }

      ssize_t cread = -1;
      size_t size = 0;
      char *frecord = NULL;
      while ((cread = getline(&frecord, &size, file)) > 0)
      {
        frecord[cread - 1] = '\0';
        push(history, frecord);
        add_history(frecord);
      }

      free(frecord);
      fclose(file);
      return;
    }

    /* Persists between calls so -a writes only records not written before. */
    static int last_wrtrec = 0;

    if (strcmp(parsed_input->argv[1], "-w") == 0)
    {
      FILE *file = fopen(parsed_input->argv[2], "w");
      if (file == NULL)
      {
        // printf("Error number %d\n", errno);
        return;
      }

      for (int i = 0; i < history->size; i++)
      {
        char *frecord = history->records[i];
        fwrite(frecord, sizeof(char), strlen(frecord), file);
        fwrite("\n", sizeof(char), 1, file);
        last_wrtrec++;
      }

      fclose(file);
    }

    if (strcmp(parsed_input->argv[1], "-a") == 0)
    {
      FILE *file = fopen(parsed_input->argv[2], "a");
      if (file == NULL)
      {
        // printf("Error number %d\n", errno);
        return;
      }

      for (int i = last_wrtrec; i < history->size; i++)
      {
        char *frecord = history->records[i];
        fwrite(frecord, sizeof(char), strlen(frecord), file);
        fwrite("\n", sizeof(char), 1, file);
        last_wrtrec++;
      }

      fclose(file);
    }

    /* A non-option argument is treated as the requested number of entries. */
    int limit = atoi(parsed_input->argv[1]);
    for (int i = history->size - limit, j = limit; j > 0; j--, i++)
      printf("\t %d %s\n", i + 1, history->records[i]);
  }
  else
  {
    for (int i = 0, j = 1; i < history->size; j++, i++)
      printf("\t %d %s\n", j, history->records[i]);
  }
}
