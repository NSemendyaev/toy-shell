#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
    // write to fds[1], read from fds[0]
    int fds[2];
    pipe(fds);

    if (fork() == 0)
    {
        close(fds[0]);
        char *args[] = {"script", NULL};
        dup2(STDOUT_FILENO, fds[1]);
        execlp("ls", "ls", (char *)NULL);
        // char *msg = "Hello World!\n";
        // write(fds[1], msg, strlen(msg));
        // close(file);
        return 0;
    }
    close(fds[1]);
    char buf[128] = {0};
    read(fds[0], buf, sizeof(buf));
    return 0;
}