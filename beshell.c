#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>


//-------------------------------------

#define SPACE_DELIMITER -1
#define REDIRECTION_SIZE 1024
#define REDIRECTION_FILE_PERMISSION 0664
#define MAXIMUM_BUILTIN_COMMANDS 10
#define MAXIMUM_HISTORY_SIZE 100


int STDOUT_ORIGINAL_FD;


//-------------------------------------
// redirection
//-------------------------------------

char redirection[REDIRECTION_SIZE];
int redirection_mode;


//-------------------------------------
// builtin Commands
//-------------------------------------

struct _my_shell_commands {
    char *command_name;
    void (*handler)(int args_count, char **command_args);
};
typedef struct _my_shell_commands Command;
Command shell_builtin_commands[MAXIMUM_BUILTIN_COMMANDS];
int commands_count = 0;



//-------------------------------------
// history 
//-------------------------------------

char *commands_history[MAXIMUM_HISTORY_SIZE];
int current = 0;



//-------------------------------------
// signatures
//-------------------------------------

size_t get_user_input(char **input, char read_from_input);
int builtin_exec(char *cmd_name, char **args, int args_count);
size_t pre_execution(char *input, size_t size, int *in_background);
int next_command(char *commands,int command_size, int end, int command_number, int fd[2], int fd_read, int fd_cmd[2]);
void add_command(Command command);
void add_to_history(char *command, int size);
void execute(char *commands, int size, int in_background);
void init_history();
void init();
void init_commands();



//----------------------------------------------------
// code
//----------------------------------------------------

void handler_cd(int args_count, char **args){
    if (args_count >= 1 && args[1] != NULL)
        chdir(args[1]);
}

void handler_history(int args_count, char **args){
   
   int counter = current;
   int line = 0;
   for (int i = 0; i < MAXIMUM_HISTORY_SIZE; i++, counter++)
   {
       if (counter >= MAXIMUM_HISTORY_SIZE)
           counter = 0;

       if (commands_history[counter] == NULL)
            continue;

       line++;
       printf(" %03d %s\n", line , commands_history[counter]);
       
   }
   
}

void handler_about(int args_count, char **args){
    system("clear");
    printf("\
  ______   _______  _______           _______  _        _       \n\
 (  ___ \\ (  ____ \\(  ____ \\|\\     /|(  ____ \\( \\      ( \\      \n\
 | (   ) )| (    \\/| (    \\/| )   ( || (    \\/| (      | (      \n\
 | (__/ / | (__    | (_____ | (___) || (__    | |      | |      \n\
 |  __ (  |  __)   (_____  )|  ___  ||  __)   | |      | |      \n\
 | (  \\ \\ | (            ) || (   ) || (      | |      | |       \n\
 | )___) )| (____/\\/\\____) || )   ( || (____/\\| (____/\\| (____/\\ \n\
 |______/ (_______/\\_______)|/     \\|(_______/(_______/(_______/ \n\
                                                               \n");

printf("\n");
printf("                     BEHRANG SHIRDEL\n");
printf("                shirdel.behrang@gmail.com\n\n\n\n");

}

void handler_help(int args_count, char **args){
    printf("\n beshell v1.0\n\n commands:\n");
    for (int i = 0; i < commands_count; i++)
    {
        Command cmd = shell_builtin_commands[i];
        printf(" - %s\n", cmd.command_name);
    }
    printf("\n");
}


void handler_exit(int args_count, char **args){
    exit(0);
}

void init_shell(){
    STDOUT_ORIGINAL_FD = dup(STDIN_FILENO);
}

void init_commands(){

    // commands

    Command cd_cmd = {"cd", &handler_cd};
    add_command(cd_cmd);

    Command history_cmd = {"history", &handler_history};
    add_command(history_cmd);

    Command about_cmd = {"about", &handler_about};
    add_command(about_cmd);

    Command exit_cmd = {"exit", &handler_exit};
    add_command(exit_cmd);


    Command help_cmd = {"help", &handler_help};
    add_command(help_cmd);
}





int main(int argc, char *argv[]){

    init_shell();
    init_commands();

    while (1)
    {
    
        init();  

        char *input = NULL;
        size_t size = 0;
        size = get_user_input(&input, 1);
        

        if (size == 0)
            continue;


        int in_background = 0;
        
        size = pre_execution(input, size, &in_background);

        char input_arr[size + 1];
        strncpy(input_arr, input, size + 1);
        free(input);

        execute(input_arr, size, in_background);
    }
        
        
    return 0;

}




void init_history(){
    for (int i = 0; i < MAXIMUM_HISTORY_SIZE; i++)
        commands_history[i] = NULL;
}

void init(){
    char host_name[40];
    char username[100];
    char current_pwd[1024];
    gethostname(host_name, 40);
    getlogin_r(username, 100);
    getcwd(current_pwd, 1024);
    printf("%s@%s:%s> ", username, host_name, current_pwd);
}




void add_to_history(char *command_text, int size){
    char *command = (char *)malloc(sizeof(char) * size);
    strcpy(command, command_text);
    
    if (current >= MAXIMUM_HISTORY_SIZE)
        current = 0;
    
    if (commands_history[current] != NULL)
        free(commands_history[current]);
    
    
    commands_history[current] = command;    
    current++;
}





void add_command(Command command){
    if (commands_count < MAXIMUM_BUILTIN_COMMANDS){
        shell_builtin_commands[commands_count] = command;
        commands_count++;
    }
}



size_t get_user_input(char **input, char read_from_input){

   
    char *in;
    size_t size = 1;
    if (read_from_input){
        in = (char *)malloc(1);
        size = getline(&in, &size, stdin);    
        in[size - 1] = '\0';
    }
    else{
        size = strlen(*input) + 1;
        in = *input;
        *input = NULL;
    }
    
    

    for (size_t i = size - 1; i >= 0; i--)
        if (in[i] == ' ')
            in[i] = '\0';
        else
            break;
   
    if (strcmp(in, "!!") == 0)
    {

        int cur_cmd = current - 1;
        if (cur_cmd < 0)
            cur_cmd = MAXIMUM_HISTORY_SIZE - 1;
        
        char *cmd = commands_history[cur_cmd];
        if (cmd != NULL){
            int s = strlen(cmd);
            *input = (char *)malloc((s + 2) * sizeof(char));
            strcpy(*input, commands_history[cur_cmd]);
            return get_user_input(input, 0);
        }else{
            printf("\nNo commands in history\n\n");
            return 0;
        }
    }
    


    *input = (char *)malloc(size);

    char flag_colon = 0;
    char flag_colon_type;
    char flag_bspace = 0;
    char *ptr = *input;
    size_t real_size = 0;

    
    
    for (size_t i = 0; i < size; i++)
    {

        if (in[i] == '\n')
            break;

        if (in[i] == '\'' || in[i] == '\"')
        {
            if (flag_colon == 0)
            {
                flag_colon = 1;
                flag_colon_type = in[i];
            }else{
                if (flag_colon_type = in[i])
                {
                    flag_colon = 0;
                    flag_colon_type = 0;
                }
            }
        }
        
        // if there is a colon then don't remove spaces
        if (flag_colon == 0)
        {
            if (in[i] == ' ')
            {
                flag_bspace = 1;
                continue;
            }else if (flag_bspace)
            {
                flag_bspace = 0;
                if (real_size != 0)
                {
                    *ptr = SPACE_DELIMITER;
                    ptr++;
                    real_size++;
                }
                
            }
        // put a space if there is space before the colon
        }else if (flag_bspace)
        {
            flag_bspace = 0;
            *ptr = SPACE_DELIMITER;
            ptr++;
            real_size++;    
        }
  
        
        
        
        *ptr = in[i];
        ptr++;
        real_size++;
            
    }
    



    // put null terminator
    *ptr = '\0';
    real_size++;


    // check if there is another space at the end
    ptr -= 2;
    if (*ptr == SPACE_DELIMITER)
    {
        *ptr = '\0';
        real_size--;
    }

    
    if (real_size > 2){
        add_to_history(in, size);
    }
    else
        free(*input);
  

    // deallocating temoorary memory
    free(in);

    return real_size - 2;

}


int builtin_exec(char *cmd_name, char **args, int args_count){
    for (int i = 0; i < commands_count; i++)
    {
        Command cmd = shell_builtin_commands[i];
        if (strcmp(cmd.command_name, cmd_name) == 0)
        {
            cmd.handler(args_count, args);
            return 1;
        }
    }

    return 0;
}




size_t pre_execution(char *input, size_t size, int *in_background){
    
     
    if (size >= 3 && input[size - 1] == '&' && input[size - 2] == SPACE_DELIMITER)
    {
         
        *in_background = 1;

        input[size - 1] = '\0';
        input[size - 2] = '\0';
        size -= 2;
    }
    for (size_t i = 0; i < REDIRECTION_SIZE; i++)
        redirection[i] = '\0';
    
    int  counter = size;
    char found = 0;
    char cot = 0;


    redirection_mode = -1;

    while (counter > 0)
    {
        if (input[counter] == '|')
            break;

        
        if (input[counter] == '"' || input[counter] == '\'')
        {
            if (input[counter] == cot)
                cot = 0;
            else
                cot = input[counter];

        }else if (input[counter] == '>')
        {
            redirection_mode = O_WRONLY | O_TRUNC;
            found = 1;
            if (input[counter-1] == '>'){
                redirection_mode = O_WRONLY | O_APPEND;
                counter--;
            }
                
            break;
        }
        counter--;
    }


    
    if (found)
    {   
        if (input[counter - 1] == SPACE_DELIMITER)
            counter--;
        
        int size2 = size;
        size = counter;
        for (int i = 0;counter < size2; counter++)
        {
            if (input[counter] != '>' && input[counter] != SPACE_DELIMITER)
            {
                redirection[i] = input[counter];
                i++;
            }    
            input[counter] = '\0';
        }
        
    }

    
    return size;
}

void execute(char *commands, int size, int in_background){


    int x[2];
    if (next_command(commands, size, 0, -1, x, 0, x) == 1)
        return;

    
    pid_t pid = fork();
    if (pid > 0)
    {
        if (in_background == 0)
            while (wait(NULL) != -1);
        return;
    }

    int end = 0;
    int fd_data[2];
    int fd_read;
    int fd_cmd[2];
    pid_t pcmd;
    int command_number = 0;
    int is_command = 1;
    
    
    pipe(fd_cmd);
    while (is_command == 1)
    {
        command_number++;
        if (command_number > 1)
            fd_read = fd_data[0];
        pipe(fd_data);
        pcmd = fork();
        if (pcmd > 0)
        {
            close(fd_data[1]);
            
            read(fd_cmd[0], &is_command, sizeof(int));
            read(fd_cmd[0], &end, sizeof(int));
            int status;
            wait(&status);
            if (command_number > 1)
                close(fd_read);
            if (status != 0)  {
                break;
            }
            continue;
        }
        break;
    }
        
    if (pcmd > 0)
        exit(0);
    

    next_command(commands, size, end, command_number, fd_data, fd_read, fd_cmd);
    
    
}


int next_command(char *commands,int command_size, int end, int command_number, int fd[2], int fd_read, int fd_cmd[2]){

    int start = end;
    char **command_args;
    
    
    char pipe_found = 0;
    while (end < command_size){
        if (end > 0 && commands[end] == '|' && commands[end - 1] == SPACE_DELIMITER && commands[end + 1] == SPACE_DELIMITER){
            pipe_found = 1;
            break;
        }
        end++;
    }
    


    int size = end - start + 1;
    char tmp[size];
    char tmp2[size];
    
    for (int i = 0; i < size; i++){
        tmp[i] = '\0';
        tmp2[i] = '\0';
    }
    


    int x = 0;
    int y = 0;
    int flag_read = 0;
    int flag = 0;
    for (int i = start; i < end; i++){
    
        if (flag)
        {
            if (commands[i] == SPACE_DELIMITER || commands[i] == '\0'){
                flag = 0;
                continue;
            }            

            tmp2[y] = commands[i];
            y++;
            continue;
        }else{
            if (commands[i] == '<')
            {
                flag = 1;
                flag_read = 1;
                if (commands[i+1] == SPACE_DELIMITER)
                    i++;
                continue;
            }
        }

        tmp[x] = commands[i];
        x++;
    }
    if (tmp[x - 1] == SPACE_DELIMITER)
        tmp[x - 1] = '\0';
    
  

    int args_count = 0;



    for (int i = 0; i < size; i++)
        if (tmp[i] == SPACE_DELIMITER)
            args_count++;
    
    
 
    args_count++;
    
    


    

    command_args = (char **)malloc((args_count + 1) * sizeof(char *));

    char delimiter = SPACE_DELIMITER;
    char *tok =  strtok(tmp, &delimiter);
    
    int counter = 0;
    
    while (tok != NULL)
    {
        size_t arg_len = strlen(tok);
        char *_s = &tok[0];


        // remove " and '
        // ex : "test" ->  test 
        if (arg_len>=2)
        {
            char *_e = &tok[arg_len - 1];
            
            if (*_s == *_e && (*_e == '"' ||  *_e == '\''))
            {
                tok[arg_len - 1] = '\0';
                *_s++;
                arg_len -= 2;   
            }
        }

        arg_len++;
        command_args[counter] = (char *)calloc(arg_len, sizeof(char));
        strncpy(command_args[counter], _s, arg_len);
        counter++;
        tok = strtok(NULL, &delimiter);       

    }

    
    
    command_args[args_count] = NULL;

    end += 2;
    int is_command = 1;

    if (end >= command_size)
        is_command = 0;

    if (command_number == -1){
        if (is_command == 0 && builtin_exec(command_args[0], command_args, args_count)){
            for (int i = 0; i < args_count; i++)
                free(command_args[i]);
            free(command_args);
            return 1;
        }
        return 0;
    }
    

    // give parent info about next command 
    close(fd_cmd[0]);
    write(fd_cmd[1], &is_command, sizeof(int));
    write(fd_cmd[1], &end, sizeof(int));
    close(fd_cmd[1]);

    

    if (is_command)
        dup2(fd[1], STDOUT_FILENO);
    else if (redirection_mode != -1)
    {
        int fileId = open(redirection, O_CREAT | redirection_mode, REDIRECTION_FILE_PERMISSION);
        dup2(fileId, STDOUT_FILENO);
    }
    
    if (flag_read)
    {
        int fileId = open(tmp2, O_RDONLY);
        dup2(fileId, STDIN_FILENO);
    }else{
        if (command_number > 1)
        {
            dup2(fd_read, STDIN_FILENO);
        }
    }
    


    // close all file discriptors
    close(fd[1]);
    close(fd[0]);
    close(fd_read);
    close(fd_cmd[0]);
    close(fd_cmd[1]);

    
    int internal_command = 1;

    if (builtin_exec(command_args[0], command_args, command_size) == 0){
        internal_command = 0;
        int result = execvp(command_args[0], command_args);
        dup2(STDOUT_ORIGINAL_FD, STDOUT_FILENO);

        if (result == -1)
            printf("\nCommand '%s' not found!\n\n", command_args[0]);
     
        fflush(stdout);
        
    }


   


    for (int i = 0; i < args_count; i++)
        free(command_args[i]);
    free(command_args);


    if (internal_command)
        exit(0);

    exit(-1);

}



