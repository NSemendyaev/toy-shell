#include "stack.h"

ST *init_stack(void)
{
    ST *stack = malloc(sizeof(ST));
    if (stack == NULL)
    {
        fprintf(stderr, "Wasn't able to initialise stack.\nError number: %d", errno);
        return NULL;
    }
    /* size is the next free slot and therefore also the record count. */
    stack->size = 0;
    return stack;
}

void push(ST *stack, char *record)
{
    if (stack->size < BUF_SIZE)
    {
        // printf("[DEBUG] Pushed to the stack: %s", record);
        /* Store a copy so callers may free or reuse their input buffer. */
        stack->records[stack->size++] = strdup(record);
    }
    return;
}

char *pop(ST *stack)
{
    if (stack->size > 0)
    {
        /* Ownership of the removed record transfers to the caller. */
        return stack->records[--(stack->size)];
    }
    return NULL;
}

void free_stack(ST *stack)
{
    /* Records are heap copies created by push(). */
    for (int i = 0; stack->records[i] != NULL; i++)
        free(stack->records[i]);
    free(stack);
}
