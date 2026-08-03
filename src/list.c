#include "list.h"

LIST *init_list(void)
{
    /* An empty list has no sentinel node; head and tail start as NULL. */
    LIST *list = malloc(sizeof(LIST));
    if (list == NULL)
    {
        printf("Error number: %d\n", errno);
        return NULL;
    }
    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    return list;
}

void append(LIST *list, int job_n, pid_t pid, char *cmd, char *status)
{
    NODE *new_node = malloc(sizeof(NODE));
    if (new_node == NULL)
    {
        printf("Error number: %d\n", errno);
        return;
    }

    new_node->job_n = job_n;
    new_node->pid = pid;
    /* The list owns copies because the caller's command buffer is short-lived. */
    new_node->cmd = strdup(cmd);
    new_node->status = strdup(status);
    new_node->next = NULL;

    if (list->head == NULL)
    {
        list->head = new_node;
        list->tail = new_node;
        list->head = list->tail;
    }
    else
    {
        /* For a non-empty list, only the former tail needs linking. */
        list->tail->next = new_node;
        list->tail = new_node;
    }

    list->size++;
}

void free_list(LIST *list)
{
    /* Save next before freeing the current node to keep traversal valid. */
    NODE *next = list->head;
    NODE *to_free = NULL;
    while (next != NULL)
    {
        to_free = next;
        next = next->next;
        free(to_free->cmd);
        free(to_free->status);
        free(to_free);
    }
    free(list);
}

void remove_l(LIST *list, int index)
{
    NODE *iter = list->head;
    NODE *prev = NULL;

    if (index == 0)
    {
        /* Removing the head advances the entry point into the list. */
        prev = iter;
        list->head = iter->next;
        free(prev->cmd);
        free(prev->status);
        free(prev);
        list->size--;
        return;
    }

    int i = 0;
    while (iter != NULL && i++ < index)
    {
        prev = iter;
        iter = iter->next;
    }
    prev->next = iter->next;

    free(iter->cmd);
    free(iter->status);
    free(iter);

    list->size--;
}

void print_list(LIST *list)
{
    NODE *iter = list->head;
    int i = 0;
    while (iter != NULL)
    {
        int status;
        bool is_done = false;
        /* WNOHANG polls a background job without blocking the shell. */
        waitpid(iter->pid, &status, WNOHANG);
        if (WIFEXITED(status))
        {
            is_done = true;
            iter->status = strdup("Done");
            iter->cmd[strlen(iter->cmd) - 1] = '\0';
        }

        if (iter->next == NULL)
        {
            printf("[%d]+  %s %24s\n", iter->job_n, iter->status, iter->cmd);
            return;
        }
        else if (iter->next->next == NULL)
        {
            printf("[%d]-  %s %24s\n", iter->job_n, iter->status, iter->cmd);
        }
        else
        {
            printf("[%d]  %s %24s\n", iter->job_n, iter->status, iter->cmd);
        }

        if (is_done)
        {
            /* Removal changes the following index, so compensate before advancing. */
            remove_l(list, i--);
        }

        iter = iter->next;
        i++;
    }
}
