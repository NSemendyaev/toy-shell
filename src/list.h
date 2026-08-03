#ifndef JOB_LIST
#define JOB_LIST

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <stdbool.h>

typedef struct Node
{
    int job_n;
    pid_t pid;
    char *cmd;       /* Owned command copy used by the jobs display. */
    char *status;    /* Owned, printable lifecycle state. */
    struct Node *next;
} NODE;

typedef struct LinkedList
{
    NODE *head;      /* First job, or NULL when the list is empty. */
    NODE *tail;      /* Last job, enabling O(1) appends. */
    size_t size;     /* Number of nodes in the list. */
} LIST;

LIST *init_list(void);
void append(LIST *list, int job_n, pid_t pid, char *cmd, char *status);
void free_list(LIST *list);
void print_list(LIST *list);
void remove_l(LIST *list, int index);

#endif
