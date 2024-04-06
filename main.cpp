#include <stdio.h>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h> 
#include "parser.h"
using namespace std;
void subshell_execution(single_input *input, bool pipe_flag);
void mixed_pipeline_execution(pipeline *pipe_inp);

void unix_error(const char *msg) {
    // fprintf(stderr, "%s\n", msg);
    exit(0);
}

pid_t Fork(void) {
    pid_t pid;
    if ((pid = fork()) < 0)
        unix_error("Fork error");
    return pid;
}

void execute_inputs(parsed_input *input) {
    pid_t pid; // Declare pid at the start

    switch (input->inputs[0].type)
    {   
        case INPUT_TYPE_SUBSHELL:
            subshell_execution(&input->inputs[0], false);
            break;
        case INPUT_TYPE_COMMAND:
            pid = Fork(); // Using your Fork wrapper for error checking
            if (pid == 0) { // Child process
                if (execvp(input->inputs->data.cmd.args[0], input->inputs->data.cmd.args) == -1) {
                    // If execvp returns, it must have failed
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
                
            } else { // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for the child to complete
            } 
            break;
        default:
            break;
    }

    // free_parsed_input(input);
}

void mixed_pipeline_execution(pipeline *pipe_inp){
    int num_pipes = pipe_inp->num_commands - 1;


    int pipefds[2 * num_pipes]; // 2 file descriptors per pipe

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i< pipe_inp->num_commands ; i++){

        pid_t pid = fork();
        if(pid < 0 ){
            perror("pipeline_exctuion: fork");
            exit(EXIT_FAILURE);
        }

        if(pid == 0) { //Child Process
            // redirect the input from prev pipe if not the first command
            if(i > 0){
                dup2(pipefds[(i-1)*2], 0);
            }

            // redirect output to the next pipe if not the last command
            if(i < num_pipes){
                dup2(pipefds[i*2 + 1], 1);
            }

            //close all pipes in the child process
            for(int j = 0; j< 2*num_pipes; j++){
                close(pipefds[j]);
            }
                     
            if(execvp(pipe_inp->commands[i].args[0], pipe_inp->commands[i].args) == -1){
                perror("pipeline_execution: execvp");
                exit(EXIT_FAILURE);
            }
            

        }
    }
    // Close all pipes in the parent process
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < pipe_inp->num_commands; i++) {
        wait(NULL);
    }
    
}


void pipeline_execution(parsed_input *input) {

    int num_pipes = input->num_inputs - 1;

    int pipefds[2 * num_pipes]; // 2 file descriptors per pipe

    for (int i = 0; i < num_pipes; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("pipe");
            exit(EXIT_FAILURE);
        }
    }

    for(int i = 0; i< input->num_inputs; i++){

        pid_t pid = fork();
        if(pid < 0 ){
            perror("pipeline_exctuion: fork");
            exit(EXIT_FAILURE);
        }

        if(pid == 0) { //Child Process

            // redirect the input from prev pipe if not the first command
            if(i > 0){
                dup2(pipefds[(i-1)*2], 0);
            }

            // redirect output to the next pipe if not the last command
            if(i < num_pipes){
                dup2(pipefds[i*2 + 1], 1);
            }

            //close all pipes in the child process
            for(int j = 0; j< 2*num_pipes; j++){
                close(pipefds[j]);
            }
            
            // execute the command
            if(input->inputs[i].type == INPUT_TYPE_COMMAND){
                // there is a mistake in here
                if(execvp(input->inputs[i].data.cmd.args[0], input->inputs[i].data.cmd.args) == -1){
                    perror("pipeline_execution: execvp");
                    exit(EXIT_FAILURE);
                }
                
            }
            else if(input->inputs[i].type == INPUT_TYPE_SUBSHELL){
                // printf("Subshell Execution\n");
                subshell_execution(&input->inputs[i], true);

            }
            else{
                ;
            }
        
        }
    }
    // Close all pipes in the parent process
    for (int i = 0; i < 2 * num_pipes; i++) {
        close(pipefds[i]);
    }

    // Wait for all child processes to finish
    for (int i = 0; i < input->num_inputs; i++) {
        wait(NULL);
    }

}


void sequential_execution(parsed_input *input) {
    for (int i = 0; i < input->num_inputs; i++) {
        single_input *curr_single_inp = &input->inputs[i];
        
        if(curr_single_inp->type == INPUT_TYPE_PIPELINE){
            mixed_pipeline_execution(&curr_single_inp->data.pline);
        }
        else if (curr_single_inp->type == INPUT_TYPE_COMMAND) {
            pid_t pid = Fork(); // Using your Fork wrapper for error checking

            if (pid == 0) { // Child process
                if (execvp(curr_single_inp->data.cmd.args[0], curr_single_inp->data.cmd.args) == -1) {
                    // If execvp returns, it must have failed
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            } else { // Parent process
                int status;
                waitpid(pid, &status, 0); // Wait for the child to complete
            }
        }
    }
}


void parallel_execution(parsed_input *input) {
    for (int i = 0; i < input->num_inputs; i++) {

        single_input *curr_single_inp = &input->inputs[i];

        if(curr_single_inp->type == INPUT_TYPE_PIPELINE){
            mixed_pipeline_execution(&curr_single_inp->data.pline);

        }
        else if (curr_single_inp->type == INPUT_TYPE_COMMAND) {
            pid_t pid = Fork(); // Using your Fork wrapper for error checking
            if (pid == 0) { // Child process
                if (execvp(curr_single_inp->data.cmd.args[0], curr_single_inp->data.cmd.args) == -1) {
                    // If execvp returns, it must have failed
                    perror("execvp");
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // Wait for all child processes to finish
    for (int i = 0; i < input->num_inputs; i++) {
        int status;
        wait(&status);
    }
}

void subshell_execution(single_input *input, bool pipe_flag){

    // printf("Subshell Execution\n");
    if(!pipe_flag){
        pid_t pid = Fork(); // Using your Fork wrapper for error checking

        if (pid == 0 ) { // Child process
            parsed_input* subshell_input = new parsed_input;

            // printf("Subshell Execution\n");
            parse_line(input->data.subshell, subshell_input);

            if(subshell_input->separator == SEPARATOR_SEQ){
                sequential_execution(subshell_input);
                exit(EXIT_SUCCESS);
            }
            else if(subshell_input->separator == SEPARATOR_PIPE){
                pipeline_execution(subshell_input);
            }
            else if(subshell_input->separator == SEPARATOR_PARA){
                ;
                
            }
            else{
                ;
            }
            free_parsed_input(subshell_input);
        
    } else { // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for the child to complete
    }
    }
    else{ // not coming from pipeline solely subshell
        parsed_input* subshell_input = new parsed_input;

        // printf("Subshell Execution\n");
        parse_line(input->data.subshell, subshell_input);         

        if(subshell_input->separator == SEPARATOR_SEQ){
            sequential_execution(subshell_input);
            exit(EXIT_SUCCESS);
        }
        else if(subshell_input->separator == SEPARATOR_PIPE){
            pipeline_execution(subshell_input);
        }
        else if(subshell_input->separator == SEPARATOR_PARA){
            // it is different than classical parallel execution i need a repeater
            // parallel_execution(&subshell_input);

            ;
        }
        else{
            ;
        }
        free_parsed_input(subshell_input);

    }

    
    
}

int main() {
    char line[INPUT_BUFFER_SIZE];
    parsed_input *input = new  parsed_input;

    while (1) {
        printf("/> ");

        if (fgets(line, INPUT_BUFFER_SIZE, stdin) == NULL) {
            break;
        }
        if (parse_line(line, input)) {

            pretty_print(input);

            // if the command is quit, exit the shell
            if( input->inputs[0].type==INPUT_TYPE_COMMAND && strcmp(input->inputs[0].data.cmd.args[0], "quit")== 0){
                free_parsed_input(input);  
                break;
            }
            // single command case
            if(input->num_inputs == 1){
                
                execute_inputs(input);
            }

            if(input->separator == SEPARATOR_SEQ)
            {
                sequential_execution(input);
            }
            else if(input->separator == SEPARATOR_PIPE){
                // printf("Pipeline Execution\n");
                pipeline_execution(input);
            }
            else if(input->separator == SEPARATOR_PARA){
                parallel_execution(input);
            }
            else{
                ;
            }
            // pretty_print(&input);
            free_parsed_input(input);
        } else {
            exit(0);
        }
    }

    return 0;
}

// ps aux | grep Z
// echo "Hello" | cat ; cat input.txt; cat input2.txt | grep "k" | tr /a-z/ /A-Z/
// echo "Hello" | cat , cat input.txt , cat input2.txt | grep "k" | tr /a-z/ /A-Z/
// cat parser.h | grep "struct" | wc -c ; cat parser.h | grep "struct" | wc -c ; echo "AAAAAA"
// cat parser.h | grep "struct" | wc -c , cat parser.h | grep "struct" | wc -c , echo "AAAAAA"
// cat parser.h | grep "struct" | wc -c , cat parser.h | grep "struct" | wc -c , echo "AAAAAA" , echo "BBBB" , echo "CCCC" , echo "World"
// (echo "Hello"; echo "World")
// cat parser.h | (grep "struct"; echo "Done") | wc -c
// ls -l , ls , ls , ls , ps aux | grep Z , cat parser.h | grep "struct" | wc -c , echo "AAAAAA" , echo "BBBB" , echo "CCCC" , echo "World"
// ( A | B , C | D | E , F , G | H )
// (cat parser.h | grep "struct" | wc -c , cat parser.h | grep "struct" | wc -c , echo "AAAAAA")
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done." ; cat parser.h | grep "struct" | wc -c ; cat parser.h | grep "struct" | wc -c ; echo "AAAAAA")
// (ls -l , ls , ls , ls , ps aux | grep Z , cat parser.h | grep "struct" | wc -c , echo "AAAAAA" , echo "BBBB" , echo "CCCC" , echo "World")