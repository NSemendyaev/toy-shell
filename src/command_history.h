#ifndef COMMAND_HISTORY_H
#define COMMAND_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#define COMMAND_HISTORY_CAPACITY 512U

/* A bounded, heap-owning record of commands used for history persistence. */
typedef struct {
    char *entries[COMMAND_HISTORY_CAPACITY];
    size_t length;
} CommandHistory;

CommandHistory *command_history_create(void);
void command_history_destroy(CommandHistory *history);
bool command_history_push(CommandHistory *history, const char *command);
char *command_history_pop(CommandHistory *history);

#endif
