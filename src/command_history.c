#include "command_history.h"

#include <stdlib.h>
#include <string.h>

static char *duplicate_string(const char *source)
{
    size_t length = strlen(source) + 1U;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, source, length);
    }

    return copy;
}

CommandHistory *command_history_create(void)
{
    /* The fixed-size entry array is zeroed so destruction is straightforward. */
    return calloc(1, sizeof(CommandHistory));
}

void command_history_destroy(CommandHistory *history)
{
    if (history == NULL) {
        return;
    }

    for (size_t entry_index = 0; entry_index < history->length; ++entry_index) {
        free(history->entries[entry_index]);
    }
    free(history);
}

bool command_history_push(CommandHistory *history, const char *command)
{
    /* Store a private copy because Readline's input buffer is freed by the caller. */
    if (history == NULL || command == NULL ||
        history->length == COMMAND_HISTORY_CAPACITY) {
        return false;
    }

    char *command_copy = duplicate_string(command);
    if (command_copy == NULL) {
        return false;
    }

    history->entries[history->length++] = command_copy;
    return true;
}

char *command_history_pop(CommandHistory *history)
{
    /* Ownership of the returned string transfers to the caller. */
    if (history == NULL || history->length == 0U) {
        return NULL;
    }

    return history->entries[--history->length];
}
