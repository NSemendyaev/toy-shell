#ifndef STACK
#define STACK

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 512

typedef struct STACK
{
    char *records[BUF_SIZE]; /* Heap-owned command history entries. */
    int size;                /* Number of populated entries. */
} ST;

ST *init_stack(void);
void push(ST *stack, char *record);
void free_stack(ST *stack);
char *pop(ST *stack);

#endif
