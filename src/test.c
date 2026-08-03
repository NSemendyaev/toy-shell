#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>

#define BUF_SIZE 512

int main(int argc, char *argv[])
{
    /* This is a standalone experiment: ls writes into a pipe and wc reads it. */
    int fds[2];
    assert(pipe(fds) == 0);

    if (fork() == 0)
    {
        /* The first child sends its standard output to the pipe's write end. */
        assert(dup2(fds[1], STDOUT_FILENO) != -1);
        char *args[] = {"ls", NULL};
        execvp("ls", args);
    }

    close(fds[1]);
    wait(NULL);

    if (fork() == 0)
    {
        close(fds[1]);

        /*char buf[BUF_SIZE];
        while (read(fds[0], buf, BUF_SIZE) > 0)
        {
            ;
        }*/

        /* The second child receives the pipe's read end as standard input. */
        dup2(fds[0], STDIN_FILENO);

        char *args[] = {"wc", NULL};
        execvp("wc", args);
    }

    return 0;
}
