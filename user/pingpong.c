#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    int parent2child[2];
    int child2parent[2];
    pipe(parent2child);
    pipe(child2parent);
    int pid = fork();
    if (pid == 0)
    {
        // child code
        const char msg = 1;
        char byte = 0;
        read(parent2child[0], &byte, 1);
        int my_pid = getpid();
        if (byte == 1)
        {
            printf("%d: received ping\n", my_pid);
        }
        else
        {
            printf("child: didn't receive\n");
        }
        write(child2parent[1], &msg, 1);
        close(parent2child[0]);
        close(parent2child[1]);
        close(child2parent[0]);
        close(child2parent[1]);
    }
    else
    {
        // parent code
        const char msg = 1;
        char byte = 0;
        write(parent2child[1], &msg, 1);
        read(child2parent[0], &byte, 1);
        int my_pid = getpid();
        if (byte == 1)
        {
            printf("%d: received pong\n", my_pid);
        }
        else
        {
            printf("parent: didn't receive\n");
        }
        close(parent2child[0]);
        close(parent2child[1]);
        close(child2parent[0]);
        close(child2parent[1]);
    }
    exit(0);
}