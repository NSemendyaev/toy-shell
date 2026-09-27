#ifndef JOB_LIST_H
#define JOB_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/* One background process tracked by the `jobs` built-in. */
typedef struct JobNode {
    int job_id;
    pid_t process_id;
    char *command;
    char *status;
    struct JobNode *next;
} JobNode;

/* Singly linked list with a tail pointer for fast appends. */
typedef struct {
    JobNode *head;
    JobNode *tail;
    size_t length;
} JobList;

JobList *job_list_create(void);
void job_list_destroy(JobList *job_list);
bool job_list_append(JobList *job_list, int job_id, pid_t process_id,
                     const char *command, const char *status);
void job_list_print(JobList *job_list);

#endif
