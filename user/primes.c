#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int is_prime(int n)
{
    for (int i = 2; i < n; i++)
    {
        if (n % i == 0)
        {
            return 0;
        }
    }
    return 1;
}

int main(int argc, char *argv[])
{
    int p[2];
    int pid = 0;
    pipe(p);
    for (int i = 2; i <= 35; i++)
    {
        write(p[1], &i, 4);
    }
    while (1)
    {
        pid = fork();
        if (pid == 0)
        {
            // child code
            int in_pipe = p[0];
            int num = 0;
            close(p[1]);
            if (!read(p[0], &num, 4))
            {
                close(p[0]);
                close(p[1]);
                exit(0);
            }
            printf("prime %d\n", num);
            // creating a new pipe for the next process
            pipe(p);
            // inserting the rest of the data into the next pipe
            int next = 0;
            while (read(in_pipe, &next, 4))
            {
                if (next % num != 0){
                    write(p[1], &next, 4);
                }
            }
            close(in_pipe);
        }
        else
        {
            close(p[0]);
            close(p[1]);
            break;
        }
    }
    while (wait(0) > 0)
        ;
    exit(0);
}