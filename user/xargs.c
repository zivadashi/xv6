#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

#define BUFF_SIZE 1024
#define MAX_ARGS MAXARG

int main(int argc, char *argv[])
{
    if (argc < 1){
        fprintf(2, "xargs: no argument passed\n");
        exit(1);
    }
    
    int count = 0;
    char* args[MAX_ARGS] = {0};
    // adding original arguments
    int i = 0;
    for (i = 1; i < argc - 1; i++){
        args[i] = argv[i + 1];
        count++;
    }

    // reading the arguments from stdin
    char buff[BUFF_SIZE] = {0};
    int len = 0;
    int curr_len = 0;
    char *curr_buff = buff;
    while ((curr_len = read(0, curr_buff, BUFF_SIZE)) > 0){
        curr_buff += curr_len;
        len += curr_len;
    }

    // copying arguments from stdin
    if (len > 0){
        args[i] = buff;
        // manually spliting the string for every ' ', '\0' and '\n'
        for (int j = 0; j < len; j++){
            if (buff[j] == ' ' || buff[j] == '\n' || buff[j] == '\0'){
                buff[j] = '\0';
                if (buff[j + 1] != '\0'){
                    i++;
                    args[i] = buff + j + 1;
                }
            }
        }
    }
    char* cmd = argv[1];
    args[0] = cmd;

    int pid = fork();
    if (pid == 0){
        // child code
        exec(cmd, args);
    }
    else{
        // parent code
        int status;
        wait(&status);
    }
    exit(0);
}