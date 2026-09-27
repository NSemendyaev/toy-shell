#include <assert.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    /* Demonstrate `ls | wc`: producer writes into one pipe end, consumer reads it. */
    int pipe_descriptors[2];
    assert(pipe(pipe_descriptors) == 0);

    /* Child one replaces stdout with the pipe's write end. */
    pid_t producer_process = fork();
    assert(producer_process >= 0);
    if (producer_process == 0) {
        assert(dup2(pipe_descriptors[1], STDOUT_FILENO) != -1);
        close(pipe_descriptors[0]);
        close(pipe_descriptors[1]);
        char *const list_arguments[] = {"ls", NULL};
        execvp(list_arguments[0], list_arguments);
        _exit(EXIT_FAILURE);
    }

    /* Child two replaces stdin with the pipe's read end. */
    pid_t consumer_process = fork();
    assert(consumer_process >= 0);
    if (consumer_process == 0) {
        assert(dup2(pipe_descriptors[0], STDIN_FILENO) != -1);
        close(pipe_descriptors[0]);
        close(pipe_descriptors[1]);
        char *const word_count_arguments[] = {"wc", NULL};
        execvp(word_count_arguments[0], word_count_arguments);
        _exit(EXIT_FAILURE);
    }

    /* The parent owns neither end, then waits so it does not leave zombies. */
    close(pipe_descriptors[0]);
    close(pipe_descriptors[1]);
    assert(waitpid(producer_process, NULL, 0) == producer_process);
    assert(waitpid(consumer_process, NULL, 0) == consumer_process);
    return EXIT_SUCCESS;
}
