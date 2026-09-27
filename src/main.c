#include "main.h"

int main(void)
{
    /* These three structures live for the complete interactive session. */
    HashTable *builtins = create_builtin_table();
    CommandHistory *history = command_history_create();
    JobList *jobs = job_list_create();

    setenv(NAME, VALUE, 0);
    const char *history_path = getenv(NAME);
    int next_job_id = 1;

    if (builtins == NULL || history == NULL || jobs == NULL)
    {
        fprintf(stderr, "Unable to initialise shell.\n");
        hash_table_destroy(builtins);
        command_history_destroy(history);
        job_list_destroy(jobs);
        return EXIT_FAILURE;
    }

    if (history_path != NULL && *history_path != '\0')
    {
        load_history_file(history, history_path);
    }

    setbuf(stdout, NULL);
    rl_bind_key('\t', rl_complete);

    for (;;)
    {
        /* readline allocates input; this loop releases it on every path. */
        char *input = readline("$ ");
        if (input == NULL)
        {
            putchar('\n');
            break;
        }
        if (*input == '\0')
        {
            free(input);
            continue;
        }

        add_history(input);
        (void)command_history_push(history, input);
        ParsedCommand *command = parse_command(input);

        //
        for (size_t i = 0; i < command->argument_count; i++)
        {
            printf("%s\n", command->arguments[i]);
        }
        //

        if (command == NULL || command->argument_count == 0U)
        {
            parsed_command_destroy(command);
            free(input);
            continue;
        }

        if (command->isExe == true)
        {
            char *pwd = getenv("PWD");
            char *abs_path = strcat(pwd, command->arguments[0]);
            if (fork() == 0)
            {
                execv(abs_path, command->arguments);
            }
            parsed_command_destroy(command);
            free(input);
            continue;
        }

        /* Built-ins execute in this process; other commands require fork/exec. */
        const char *command_name = command->arguments[0];
        const char *builtin_name = hash_table_get(builtins, command_name);
        FILE *redirection_file = NULL;
        int saved_descriptor = -1;

        if (command->redirection != NULL)
        {
            /* Open once. A child inherits it; a built-in temporarily replaces its fd. */
            const char *mode = command->redirection->append ? "a" : "w";
            redirection_file = fopen(command->redirection->path, mode);
            if (redirection_file == NULL)
            {
                fprintf(stderr, "%s: %s\n", command->redirection->path,
                        strerror(errno));
                parsed_command_destroy(command);
                free(input);
                continue;
            }
            if (builtin_name != NULL)
            {
                saved_descriptor = dup(command->redirection->file_descriptor);
                if (saved_descriptor == -1 ||
                    dup2(fileno(redirection_file),
                         command->redirection->file_descriptor) == -1)
                {
                    fprintf(stderr, "redirection: %s\n", strerror(errno));
                    if (saved_descriptor != -1)
                    {
                        close(saved_descriptor);
                    }
                    fclose(redirection_file);
                    parsed_command_destroy(command);
                    free(input);
                    continue;
                }
            }
        }

        /* Dispatch built-ins before falling back to an executable found in PATH. */
        bool should_exit = false;
        if (builtin_name != NULL && strcmp(builtin_name, "exit") == 0)
        {
            should_exit = true;
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "history") == 0)
        {
            handle_history_command(command, history);
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "jobs") == 0)
        {
            job_list_print(jobs);
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "echo") == 0)
        {
            for (size_t argument_index = 1U;
                 argument_index < command->argument_count; ++argument_index)
            {
                printf("%s%s", argument_index == 1U ? "" : " ",
                       command->arguments[argument_index]);
            }
            putchar('\n');
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "type") == 0)
        {
            if (command->argument_count < 2U)
            {
                puts(": not found");
            }
            else if (hash_table_get(builtins, command->arguments[1]) != NULL)
            {
                printf("%s is a shell builtin\n", command->arguments[1]);
            }
            else
            {
                char *executable_path = find_executable_path(command->arguments[1]);
                if (executable_path == NULL)
                {
                    printf("%s: not found\n", command->arguments[1]);
                }
                else
                {
                    printf("%s is %s\n", command->arguments[1], executable_path);
                    free(executable_path);
                }
            }
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "cd") == 0)
        {
            change_directory(command);
        }
        else if (builtin_name != NULL && strcmp(builtin_name, "pwd") == 0)
        {
            print_working_directory();
        }
        else
        {
            char *executable_path = find_executable_path(command_name);
            if (executable_path == NULL)
            {
                printf("%s: command not found\n", command_name);
            }
            else
            {
                bool is_background =
                    command->argument_count > 1U &&
                    strcmp(command->arguments[command->argument_count - 1U], "&") == 0;
                if (is_background)
                {
                    free(command->arguments[--command->argument_count]);
                    command->arguments[command->argument_count] = NULL;
                }

                /* The parent remains the shell; execv replaces only the child. */
                pid_t child_process = fork();
                if (child_process == -1)
                {
                    fprintf(stderr, "fork: %s\n", strerror(errno));
                }
                else if (child_process == 0)
                {
                    if (redirection_file != NULL &&
                        dup2(fileno(redirection_file),
                             command->redirection->file_descriptor) == -1)
                    {
                        fprintf(stderr, "redirection: %s\n", strerror(errno));
                        _exit(EXIT_FAILURE);
                    }
                    execv(executable_path, command->arguments);
                    fprintf(stderr, "%s: %s\n", command_name, strerror(errno));
                    _exit(127);
                }
                else if (is_background)
                {
                    (void)job_list_append(jobs, next_job_id, child_process, input, "Running");
                    printf("[%d] %d\n", next_job_id, child_process);
                    ++next_job_id;
                }
                else
                {
                    (void)waitpid(child_process, NULL, 0);
                }
                free(executable_path);
            }
        }

        /* Return the shell's standard descriptor to its original destination. */
        if (saved_descriptor != -1)
        {
            (void)dup2(saved_descriptor, command->redirection->file_descriptor);
            close(saved_descriptor);
        }
        if (redirection_file != NULL)
        {
            fclose(redirection_file);
        }
        parsed_command_destroy(command);
        free(input);

        if (should_exit)
        {
            break;
        }
    }

    /* Keep persistence optional: an unset HISTFILE means no file is touched. */
    if (history_path != NULL && *history_path != '\0')
    {
        write_history_file(history, history_path, "w", 0U);
    }
    clear_history();
    job_list_destroy(jobs);
    command_history_destroy(history);
    hash_table_destroy(builtins);
    return EXIT_SUCCESS;
}

static bool append_argument(ParsedCommand *command, const char *token)
{
    /* execv expects a NULL-terminated argv, so reserve one final array slot. */
    if (command->argument_count + 1U >= MAX_ARGUMENTS)
    {
        return false;
    }
    size_t length = strlen(token) + 1U;
    char *argument = malloc(length);
    if (argument == NULL)
    {
        return false;
    }
    memcpy(argument, token, length);
    command->arguments[command->argument_count++] = argument;
    command->arguments[command->argument_count] = NULL;
    return true;
}

static ParsedCommand *parse_command(const char *input)
{
    ParsedCommand *command = calloc(1, sizeof(*command));
    command->isExe = false;
    char token[INPUT_TOKEN_CAPACITY];
    size_t token_length = 0U;
    char quote = '\0';

    if (command == NULL)
    {
        return NULL;
    }

    /* Build one token at a time while tracking whether spaces are quoted. */
    for (const char *character = input; *character != '\0'; ++character)
    {
        if (command->argument_count == 0 && *character == '.')
        {
            command->isExe = true;
            continue;
        }

        if (quote == '\0' && (*character == ' ' || *character == '\t'))
        {
            if (token_length > 0U)
            {
                token[token_length] = '\0';
                if (!append_argument(command, token))
                {
                    parsed_command_destroy(command);
                    return NULL;
                }
                token_length = 0U;
            }
            continue;
        }
        if (*character == '\\' && quote != '\'')
        {
            if (character[1] != '\0')
            {
                ++character;
            }
        }
        else if (*character == '\'' || *character == '"')
        {
            if (quote == '\0')
            {
                quote = *character;
                continue;
            }
            if (quote == *character)
            {
                quote = '\0';
                continue;
            }
        }
        /* A redirection target is parsed as the final argument, then removed below. */
        if (quote == '\0' && *character == '>')
        {
            if (token_length == 1U && (token[0] == '1' || token[0] == '2'))
            {
                command->redirection = calloc(1, sizeof(*command->redirection));
                if (command->redirection == NULL)
                {
                    parsed_command_destroy(command);
                    return NULL;
                }
                command->redirection->file_descriptor = token[0] - '0';
                token_length = 0U;
            }
            else
            {
                if (token_length > 0U)
                {
                    token[token_length] = '\0';
                    if (!append_argument(command, token))
                    {
                        parsed_command_destroy(command);
                        return NULL;
                    }
                    token_length = 0U;
                }
                command->redirection = calloc(1, sizeof(*command->redirection));
                if (command->redirection == NULL)
                {
                    parsed_command_destroy(command);
                    return NULL;
                }
                command->redirection->file_descriptor = STDOUT_FILENO;
            }
            if (character[1] == '>')
            {
                command->redirection->append = true;
                ++character;
            }
            continue;
        }
        if (token_length + 1U >= sizeof(token))
        {
            fprintf(stderr, "Command is too long.\n");
            parsed_command_destroy(command);
            return NULL;
        }
        token[token_length++] = *character;
    }

    if (token_length > 0U)
    {
        token[token_length] = '\0';
        if (!append_argument(command, token))
        {
            parsed_command_destroy(command);
            return NULL;
        }
    }
    if (quote != '\0')
    {
        fprintf(stderr, "Unterminated quote.\n");
        parsed_command_destroy(command);
        return NULL;
    }
    if (command->redirection != NULL)
    {
        if (command->argument_count == 0U)
        {
            fprintf(stderr, "Redirection requires a target file.\n");
            parsed_command_destroy(command);
            return NULL;
        }
        command->redirection->path = command->arguments[--command->argument_count];
        command->arguments[command->argument_count] = NULL;
    }
    return command;
}

static void parsed_command_destroy(ParsedCommand *command)
{
    /* This is the single owner for allocations made while parsing a line. */
    if (command == NULL)
    {
        return;
    }
    for (size_t argument_index = 0U; argument_index < command->argument_count;
         ++argument_index)
    {
        free(command->arguments[argument_index]);
    }
    if (command->redirection != NULL)
    {
        free(command->redirection->path);
        free(command->redirection);
    }
    free(command);
}

static HashTable *create_builtin_table(void)
{
    /* Values repeat the keys: callers only need an efficient membership lookup. */
    static const char *const builtin_names[] = {
        "echo", "exit", "type", "cd", "pwd", "history", "jobs"};
    HashTable *builtins = hash_table_create();
    if (builtins == NULL)
    {
        return NULL;
    }
    for (size_t name_index = 0U;
         name_index < sizeof(builtin_names) / sizeof(builtin_names[0]); ++name_index)
    {
        if (!hash_table_set(builtins, builtin_names[name_index],
                            (void *)builtin_names[name_index]))
        {
            hash_table_destroy(builtins);
            return NULL;
        }
    }
    return builtins;
}

static char *find_executable_path(const char *command_name)
{
    /* strtok_r mutates its input, so search a private copy of PATH. */
    const char *path_environment = getenv("PATH");
    if (command_name == NULL || path_environment == NULL)
    {
        return NULL;
    }
    size_t path_length = strlen(path_environment) + 1U;
    char *path_copy = malloc(path_length);
    if (path_copy == NULL)
    {
        return NULL;
    }
    memcpy(path_copy, path_environment, path_length);

    char *path_context = NULL;
    for (char *directory = strtok_r(path_copy, ":", &path_context);
         directory != NULL;
         directory = strtok_r(NULL, ":", &path_context))
    {
        size_t candidate_length = strlen(directory) + strlen(command_name) + 2U;
        char *candidate = malloc(candidate_length);
        if (candidate == NULL)
        {
            continue;
        }
        (void)snprintf(candidate, candidate_length, "%s/%s", directory, command_name);
        if (access(candidate, X_OK) == 0)
        {
            free(path_copy);
            return candidate;
        }
        free(candidate);
    }
    free(path_copy);
    return NULL;
}

static void change_directory(const ParsedCommand *command)
{
    /* `cd` without a parameter, and `~`, both resolve through HOME. */
    const char *path = command->argument_count < 2U ? getenv("HOME") : command->arguments[1];
    if (path == NULL || *path == '\0')
    {
        fputs("cd: HOME is not set\n", stderr);
        return;
    }
    if (path[0] == '~' && (path[1] == '\0' || path[1] == '/'))
    {
        const char *home = getenv("HOME");
        if (home != NULL)
        {
            size_t expanded_length = strlen(home) + strlen(path) + 1U;
            char *expanded_path = malloc(expanded_length);
            if (expanded_path != NULL)
            {
                (void)snprintf(expanded_path, expanded_length, "%s%s", home, path + 1);
                path = expanded_path;
            }
        }
    }
    if (chdir(path) == -1)
    {
        fprintf(stderr, "cd: %s: %s\n", path, strerror(errno));
    }
    else
    {
        char working_directory[PATH_MAX];
        if (getcwd(working_directory, sizeof(working_directory)) != NULL)
        {
            (void)setenv("PWD", working_directory, 1);
        }
    }
    if (path != command->arguments[1] && path != getenv("HOME"))
    {
        free((void *)path);
    }
}

static void print_working_directory(void)
{
    char working_directory[PATH_MAX];
    if (getcwd(working_directory, sizeof(working_directory)) == NULL)
    {
        fprintf(stderr, "pwd: %s\n", strerror(errno));
        return;
    }
    puts(working_directory);
}

static void load_history_file(CommandHistory *history, const char *path)
{
    /* Add loaded lines to both our persistence list and Readline's UI history. */
    FILE *history_file = fopen(path, "r");
    char *line = NULL;
    size_t line_capacity = 0U;
    ssize_t line_length;
    if (history_file == NULL)
    {
        return;
    }
    while ((line_length = getline(&line, &line_capacity, history_file)) != -1)
    {
        if (line_length > 0 && line[line_length - 1] == '\n')
        {
            line[line_length - 1] = '\0';
        }
        if (!command_history_push(history, line))
        {
            break;
        }
        add_history(line);
    }
    free(line);
    fclose(history_file);
}

static void write_history_file(const CommandHistory *history, const char *path,
                               const char *mode, size_t first_entry)
{
    /* first_entry is zero for `-w` and the last saved position for `-a`. */
    FILE *history_file = fopen(path, mode);
    if (history_file == NULL)
    {
        fprintf(stderr, "history: %s: %s\n", path, strerror(errno));
        return;
    }
    for (size_t entry_index = first_entry; entry_index < history->length; ++entry_index)
    {
        fprintf(history_file, "%s\n", history->entries[entry_index]);
    }
    fclose(history_file);
}

static void handle_history_command(const ParsedCommand *command,
                                   CommandHistory *history)
{
    /* Tracks the boundary needed by `history -a` across built-in invocations. */
    static size_t written_entries = 0U;
    const char *history_path = command->argument_count > 2U ? command->arguments[2] : getenv("HISTFILE");

    if (command->argument_count > 1U && strcmp(command->arguments[1], "-r") == 0)
    {
        if (history_path != NULL)
        {
            load_history_file(history, history_path);
        }
        return;
    }
    if (command->argument_count > 1U && strcmp(command->arguments[1], "-w") == 0)
    {
        if (history_path != NULL)
        {
            write_history_file(history, history_path, "w", 0U);
            written_entries = history->length;
        }
        return;
    }
    if (command->argument_count > 1U && strcmp(command->arguments[1], "-a") == 0)
    {
        if (history_path != NULL)
        {
            write_history_file(history, history_path, "a", written_entries);
            written_entries = history->length;
        }
        return;
    }

    size_t first_entry = 0U;
    if (command->argument_count > 1U)
    {
        char *end = NULL;
        long requested_entries = strtol(command->arguments[1], &end, 10);
        if (*command->arguments[1] != '\0' && *end == '\0' && requested_entries > 0 &&
            (size_t)requested_entries < history->length)
        {
            first_entry = history->length - (size_t)requested_entries;
        }
    }
    for (size_t entry_index = first_entry; entry_index < history->length; ++entry_index)
    {
        printf("\t %zu %s\n", entry_index + 1U, history->entries[entry_index]);
    }
}
