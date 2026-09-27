#ifndef MAIN
#define MAIN

#define _POSIX_C_SOURCE 200809L

#include "command_history.h"
#include "hash_table.h"
#include "job_list.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <readline/history.h>
#include <readline/readline.h>

#define INPUT_TOKEN_CAPACITY 2048U
#define MAX_ARGUMENTS 512U

/* One output redirection parsed from a command, such as `2>>errors.log`. */
typedef struct
{
    int file_descriptor;
    char *path;
    bool append;
} Redirection;

/* The parser owns every string in arguments and the optional redirection. */
typedef struct
{
    size_t argument_count;
    char *arguments[MAX_ARGUMENTS];
    Redirection *redirection;
} ParsedCommand;

static ParsedCommand *parse_command(const char *input); // static on a c function makes it private to that .c file
static void parsed_command_destroy(ParsedCommand *command);
static HashTable *create_builtin_table(void);
static char *find_executable_path(const char *command_name);
static void change_directory(const ParsedCommand *command);
static void print_working_directory(void);
static void handle_history_command(const ParsedCommand *command,
                                   CommandHistory *history);
static void load_history_file(CommandHistory *history, const char *path);
static void write_history_file(const CommandHistory *history, const char *path,
                               const char *mode, size_t first_entry);
#endif
