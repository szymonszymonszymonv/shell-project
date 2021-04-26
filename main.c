#include <stdio.h>
#include <unistd.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>

char** split_command(char* command, char *delim){
    int num_elem = 1;
    int whitespace = 0;
    // count how many elements will be in an array based on ' ' and '|' in text
    // if we encounter whitespace, set whitespace to 1
    // need to avoid situations, where " | " would count too many additional elements
    for(int i = 0; i < (int)strlen(command); i++){
        if(command[i] == ' ' || command[i] == '|'){
            if(whitespace == 0){
                num_elem++;
            }
            whitespace = 1;
            continue;
        }
        whitespace = 0;
    }
    char **splitted = calloc(num_elem + 1, sizeof(char*));
    char *s = strtok(command, delim);
    for(int i = 0; i < num_elem; i++){
        splitted[i] = s;
        s = strtok(NULL, delim);
    }

    splitted[num_elem] = NULL;
    return splitted;
}

char* get_input(char *prompt){
    char *command = readline(prompt);
//    printf("%s", command);
    return command;
}


void exec_command_background(char **args){
    pid_t pid, sid;

    pid = fork();
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    sid = setsid();
    if (sid < 0) {
        exit(EXIT_FAILURE);
    }


    if ((chdir("/")) < 0) {
        exit(EXIT_FAILURE);
    }

    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);


    int process_id = fork();
    if(process_id < 0)
        exit(EXIT_FAILURE);
    else if(process_id == 0)
    {
        execvp(args[0], args);
        while(1);
        exit(EXIT_SUCCESS);
    }

}

void exec_command(char **args){
    pid_t sid, pid = fork();
    int status;

    if(pid == 0){

        execvp( args[0], args);
        fflush(stdout);
        printf("ERROR: %s\n", strerror(errno));
        exit(0);
    }

    if(pid > 0) {
        waitpid(pid, &status, 0);
    }

}

void exec_command_pipe(char **args){


}

void cd(char *path){
    if(chdir(path) != 0){
        fflush(stdout);
        printf("ERROR: %s\n", strerror(errno));
    }
}

void update_prompt(char *prompt, char *username){
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    char *tmp = malloc(512 * sizeof(char));
    sprintf(tmp, "%s @ %s\n>_ ", username, cwd);
    strcpy(prompt, tmp);
    free(tmp);
}

int main()
{
    char *username = getlogin();
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    char *prompt = malloc(512 * sizeof(char));
    sprintf(prompt, "%s @ %s\n>_ ", username, cwd);

    while(1){
        char *command = get_input(prompt);
        char **args = split_command(command, " &");
        int background = 0;
        // look for '&' to see if the command will run in background
        if(strchr(command, '&') != NULL){
            background = 1;
        }
        // special case: if command is cd:
        if(strcmp("cd", args[0]) == 0){
            cd(args[1]);
            update_prompt(prompt, username);
            continue;
        }
        if(background == 0){
            exec_command(args);
        }
        else{
            exec_command_background(args);
        }
    }

}