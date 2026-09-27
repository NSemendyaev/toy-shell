#include "job_list.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

static char *duplicate_string(const char *source)
{
    size_t length = strlen(source) + 1U;
    char *copy = malloc(length);

    if (copy != NULL) {
        memcpy(copy, source, length);
    }

    return copy;
}

static void job_node_destroy(JobNode *job)
{
    /* Each node owns the command and status strings copied on insertion. */
    free(job->command);
    free(job->status);
    free(job);
}

static void job_list_remove_after(JobList *job_list, JobNode *previous_job)
{
    /* A NULL previous node means that the head is being removed. */
    JobNode *job = previous_job == NULL ? job_list->head : previous_job->next;
    if (previous_job == NULL) {
        job_list->head = job->next;
    } else {
        previous_job->next = job->next;
    }
    if (job == job_list->tail) {
        job_list->tail = previous_job;
    }
    --job_list->length;
    job_node_destroy(job);
}

JobList *job_list_create(void)
{
    /* An empty list has no sentinel node; both endpoints begin as NULL. */
    return calloc(1, sizeof(JobList));
}

void job_list_destroy(JobList *job_list)
{
    if (job_list == NULL) {
        return;
    }

    JobNode *job = job_list->head;
    while (job != NULL) {
        JobNode *next_job = job->next;
        job_node_destroy(job);
        job = next_job;
    }
    free(job_list);
}

bool job_list_append(JobList *job_list, int job_id, pid_t process_id,
                     const char *command, const char *status)
{
    if (job_list == NULL || command == NULL || status == NULL) {
        return false;
    }

    JobNode *new_job = calloc(1, sizeof(*new_job));
    if (new_job == NULL) {
        return false;
    }
    new_job->command = duplicate_string(command);
    new_job->status = duplicate_string(status);
    if (new_job->command == NULL || new_job->status == NULL) {
        job_node_destroy(new_job);
        return false;
    }

    new_job->job_id = job_id;
    new_job->process_id = process_id;
    /* Keeping a tail pointer makes background-job insertion constant time. */
    if (job_list->tail == NULL) {
        job_list->head = new_job;
    } else {
        job_list->tail->next = new_job;
    }
    job_list->tail = new_job;
    ++job_list->length;
    return true;
}

void job_list_print(JobList *job_list)
{
    JobNode *previous_job = NULL;
    JobNode *job = job_list == NULL ? NULL : job_list->head;

    while (job != NULL) {
        JobNode *next_job = job->next;
        int wait_status = 0;
        /* WNOHANG checks status without pausing the interactive shell. */
        pid_t result = waitpid(job->process_id, &wait_status, WNOHANG);
        bool job_finished = result == job->process_id && WIFEXITED(wait_status);

        if (job_finished) {
            free(job->status);
            job->status = duplicate_string("Done");
        }

        /* Match common shell notation: newest job '+', previous job '-'. */
        const char *marker = next_job == NULL ? "+" :
                             next_job->next == NULL ? "-" : " ";
        printf("[%d]%s  %s %24s\n", job->job_id, marker, job->status, job->command);

        if (job_finished) {
            job_list_remove_after(job_list, previous_job);
        } else {
            previous_job = job;
        }
        job = next_job;
    }
}
