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
#include <fcntl.h>


struct command_info{
    char **args;
    int pipe;
    int background;
    int pipe_num;
    int redirect;
};

void print_history(int signum){
    HIST_ENTRY **h_list = history_list();
    for(int i = 0; h_list[i] != NULL; i++){
        printf("%s\n", h_list[i]->line);
    }
    exit(0);
}

void store_history(int signum){
    char *home = getenv("HOME");
    char path[256];
    sprintf(path, "%s/.s_history.txt", home);
    int fd = open(path, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    printf("writing to %s\n", path);
    append_history(20, path);
    close(fd);
    exit(0);
}

char* remove_whitespace(char* s) {
    while(isspace((unsigned char)*s)){
        s++;
    }
    char *end = s + strlen(s) - 1;
    while(end > s && isspace((unsigned char)*end)){
        end--;
    }

    end[1] = '\0';
    return s;

}

struct command_info split_command(char* command){
    int num_elem = 1;
    int whitespace = 0;
    int background = 0;
    int pipe = 0;
    int redirect = 0;
    char *delim;

    if(strchr(command, '|') != NULL){
        pipe = 1;
    }
    if(strchr(command, '&') != NULL){
        background = 1;
    }
    if(strchr(command, '>') != NULL){
        redirect = 1;
    }

    if(pipe){
        delim = "|";
    }
    else if(background){
        delim = " &";
    }
    else if(redirect){
        delim = ">";
    }
    else{
        delim = " ";
    }
    // copy command to tmp, so that the original command isn't manipulated by strtok()
    char *tmp = malloc(strlen(command) * sizeof(char));
    strcpy(tmp, command);
    // count how many elements will be in an array based on ' ' and '|' in text
    // if we encounter whitespace, set whitespace to 1
    // need to avoid situations, where " | " would count too many additional elements
    for(int i = 0; i < (int)strlen(command); i++){
        if(pipe == 1){
            if(command[i] == '|'){
                num_elem++;
            }
        }
        else if(redirect == 1){
            if(command[i] == '>'){
                num_elem++;
            }
        }
        else if(command[i] == ' '){
            if(whitespace == 0){
                num_elem++;
            }
            whitespace = 1;
            continue;
        }
        whitespace = 0;
    }
    char **splitted = calloc(num_elem + 1, sizeof(char*));
    char *s = strtok(tmp, delim);
    for(int i = 0; i < num_elem; i++){
        splitted[i] = s;
        s = strtok(NULL, delim);
    }

    splitted[num_elem] = NULL;
    struct command_info ret;
    ret.args = splitted;
    ret.background = background;
    ret.pipe = pipe;
    ret.pipe_num = num_elem - 1;
    ret.redirect = redirect;
    return ret;
}

char* get_input(char *prompt){
    while(!feof(stdin)){
        char *command = readline(prompt);
        if(command == NULL){
            break;
        }
        return command;
    }
    raise(SIGHUP);
    return 0;
}

void exec_command_background(char **args){
    pid_t pid;
    pid = fork();

    if(pid == 0){
        execvp(args[0], args);
        exit(1);
    }
    else{
        waitpid(-1, NULL, WNOHANG);
    }
    fflush(stdout);


}

void exec_command(char **args){
    pid_t pid = fork();
    int *status = malloc(sizeof(int));

    if(pid == 0){
        execvp( args[0], args);
        fflush(stdout);
        printf("ERROR: %s\n", strerror(errno));
        exit(0);
    }

    if(pid > 0) {
        waitpid(pid, status, 0);
    }

}

void exec_command_pipe(char **args, int pipe_num){
    int pipefd[2 * pipe_num];
    pid_t pid;
    for(int i = 0; i < pipe_num; i++){
        pipe(pipefd + i * 2);
    }

    int fd_num = 0;
    for(int i = 0; args[i] != NULL; i++, fd_num += 2){
        struct command_info c_info = split_command(args[i]);
        char **comm = c_info.args;
        pid = fork();
        if(pid == 0){
            if(args[i+1] != NULL){
                dup2(pipefd[fd_num + 1], 1);
            }
            if(fd_num > 0){
                dup2(pipefd[fd_num - 2], 0);
            }
            for(int j = 0; j < pipe_num * 2; j++){
                close(pipefd[j]);
            }
            execvp(comm[0], comm);
            exit(1);
        }
    }
    for(int i = 0; i < pipe_num * 2; i++){
        close(pipefd[i]);
    }
    for(int i = 0; i < pipe_num + 1; i++){
        wait(NULL);
    }

}

void exec_command_redirect(char **args){
    char *fname = args[1];
    fname = remove_whitespace(fname);
    struct command_info c_info = split_command(args[0]);
    char **arguments = c_info.args;

    pid_t pid = fork();
    int *status = malloc(sizeof(int));

    if(pid == 0){
        int fd = open(fname, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

        dup2(fd, 1);
        dup2(fd, 2);

        close(fd);
        execvp( arguments[0], arguments);
        printf("EXEC ERROR: %s\n", strerror(errno));
        exit(0);
    }

    if(pid > 0) {
        waitpid(pid, status, 0);
    }
    fflush(stdout);

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

int is_empty(char *str){
    char *tmp = remove_whitespace(str);
    if(tmp[0] == '\0'){
        return 1;
    }
    else{
        return 0;
    }
}

int main()
{
    signal(SIGQUIT, print_history);
    signal(SIGHUP, store_history);
    using_history();
    char *username = getlogin();
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    char *prompt = malloc(512 * sizeof(char));
    sprintf(prompt, "%s @ %s\n>_ ", username, cwd);

    while(!feof(stdin)){
        // wait for any stopped children
        waitpid(-1, NULL, WUNTRACED);
        fflush(stdout);

        char *command = get_input(prompt);
        // if command is just whitespace: ignore and continue
        if(is_empty(command)){
            continue;
        }
        add_history(command);
        struct command_info c_info = split_command(command);

        char **args = c_info.args;
        int background = c_info.background;
        int pipe = c_info.pipe;
        int pipe_num = c_info.pipe_num;
        int redirect = c_info.redirect;
        // special case: if command is cd:
        if(strcmp("cd", args[0]) == 0){
            cd(args[1]);
            update_prompt(prompt, username);
            continue;
        }
        if(redirect){
            exec_command_redirect(args);
        }
        else if(background){
            exec_command_background(args);
        }
        else if(pipe){
            exec_command_pipe(args, pipe_num);
        }
        else{
            exec_command(args);
        }
    }
    printf("WYSZEDLEM POZDRO");

}