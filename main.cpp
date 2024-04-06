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
            // If it is a pipeline, execute the pipeline (see Pipeline Execution). Do not wait for the pipeline to finish.
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

void repeater(int pipefds[], int num_pipes){
    // repeaters fork , repeater code
    pid_t pid = fork();
    if(pid < 0 ){
        perror("pipeline_exctuion: fork");
        exit(EXIT_FAILURE);
    }
    // for each line of stdin replicate to pipe_*[1]
    if(pid == 0) { //Child Process

        // closing the read ends of all pips
        for (int i = 0; i < num_pipes; i++) {
            close(pipefds[i * 2]); // Close the read end of each pipe
        }

        char line[INPUT_BUFFER_SIZE];

        // reading from stdin
        while (fgets(line, INPUT_BUFFER_SIZE, stdin) != NULL) {
            // write to all pipes
            for (int i = 0; i < num_pipes; i++) {
                write(pipefds[i * 2 + 1], line, strlen(line));
            }
        }

        // eof is reached close all pipes
        for (int i = 0; i < num_pipes; i++) {
            close(pipefds[i * 2 + 1]); // Close the write end of each pipe
        }
        exit(0);
    }
                        
}

void subshell_execution(single_input *input, bool pipe_flag){
    parsed_input* subshell_input = new parsed_input;

    parse_line(input->data.subshell, subshell_input);

    if(!pipe_flag){
        pid_t pid = Fork(); // Using your Fork wrapper for error checking

        if (pid == 0 ) { // Child process

            if(subshell_input->separator == SEPARATOR_SEQ){
                sequential_execution(subshell_input);
                exit(EXIT_SUCCESS);
            }
            else if(subshell_input->separator == SEPARATOR_PIPE){
                pipeline_execution(subshell_input);
                exit(EXIT_SUCCESS);
            }
            else if(subshell_input->separator == SEPARATOR_PARA){
                // (A, B , C)
               int loop_count = subshell_input->num_inputs+1; // 1 more for the repeater

                // we need pipe for the amount of inputs
                int num_inps = subshell_input->num_inputs; // number of pipes needed for the inputs
                int pipefds[2 * num_inps]; // file descriptors per pipe

                // loop for the inputs pipe creation and direction for the stdin handled here
               for(int i = 0; i< num_inps; i++){
                    // pipe_i = pipe() for each input one pipe
                    if (pipe(pipefds + i * 2) == -1) {
                            perror("pipe");
                            exit(EXIT_FAILURE);
                    }
                    pid_t pid = fork();
                    if(pid < 0 ){
                        perror("pipeline_exctuion: fork");
                        exit(EXIT_FAILURE);
                    }
                    if(pid == 0){
                        close(pipefds[i*2 + 1]); // close write end of the pipe
                        dup2(pipefds[i*2], 0); // read from pipe

                        close(pipefds[i*2]); // close write end of the pipe

     
                        // it will write to stdout
                        // execute the command 
                        single_input *curr_single_inp = &subshell_input->inputs[i];

                        if(curr_single_inp->type == INPUT_TYPE_PIPELINE){
                            // If it is a pipeline, execute the pipeline (see Pipeline Execution). Do not wait for the pipeline to finish.
                            mixed_pipeline_execution(&curr_single_inp->data.pline);
                            exit(EXIT_SUCCESS);

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

                }
                
                // repeater code
                repeater(pipefds, num_inps);

                // here is subshell process
                // Close all pipes in the parent process
                for (int i = 0; i < 2 * num_inps; i++) {
                    close(pipefds[i]);
                }

                // Wait for all child processes to finish
                for (int i = 0; i < loop_count; i++) {
                    wait(NULL);
                }
                exit(EXIT_SUCCESS);

            }   
        
        } else { // Parent process
            
            int status;
            waitpid(pid, &status, 0); // Wait for the child to complete

        }
        
    }else{
        // if its coming from pipeline
            ;
            /* if(subshell_input->separator == SEPARATOR_SEQ){
                sequential_execution(subshell_input);
                exit(EXIT_SUCCESS);
            }
            else if(subshell_input->separator == SEPARATOR_PIPE){
                pipeline_execution(subshell_input);
            }
            else if(subshell_input->separator == SEPARATOR_PARA){
                // (B , C)
               int loop_count = subshell_input->num_inputs+1; // 1 more for the repeater

                // we need pipe for the amount of inputs
                int num_inps = subshell_input->num_inputs; // number of pipes needed for the inputs
                int pipefds[2 * num_inps]; // file descriptors per pipe

                // loop for the inputs pipe creation and direction for the stdin handled here
               for(int i = 0; i< num_inps; i++){
                    // pipe_i = pipe() for each input one pipe
                    if (pipe(pipefds + i * 2) == -1) {
                            perror("pipe");
                            exit(EXIT_FAILURE);
                    }
                    pid_t pid = fork();
                    if(pid < 0 ){
                        perror("pipeline_exctuion: fork");
                        exit(EXIT_FAILURE);
                    }
                    if(pid == 0){
                        dup2(pipefds[i*2], 0); // read from pipe

                        close(pipefds[i*2 + 1]); // close write end of the pipe
                        // it will write to stdout
                        // execute the command 
                        single_input *curr_single_inp = &subshell_input->inputs[i];

                        if(curr_single_inp->type == INPUT_TYPE_PIPELINE){
                            // If it is a pipeline, execute the pipeline (see Pipeline Execution). Do not wait for the pipeline to finish.
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

                }
                
                // repeater code
                repeater(pipefds, num_inps);

                // here is parent process
                // Close all pipes in the parent process
                for (int i = 0; i < 2 * num_inps; i++) {
                    close(pipefds[i]);
                }

                // Wait for all child processes to finish
                for (int i = 0; i < loop_count; i++) {
                    wait(NULL);
                }


            }
            else{
                ;
            } */

    }

    
    free_parsed_input(subshell_input);
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
            /* if(input->num_inputs == 1){
                
                execute_inputs(input);
            } */

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
            else if(input->separator == SEPARATOR_NONE){
                if(input->inputs[0].type == INPUT_TYPE_SUBSHELL){
                    subshell_execution(&input->inputs[0], false);
                }
                else{
                    execute_inputs(input);
                }

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
// (echo "Hello", echo "World")
// cat parser.h | (grep "struct"; echo "Done") | wc -c
// A,B,C,D, E | F , G | H , I, J
// ls -l , ls , ls , ls , ps aux | grep Z , cat parser.h | grep "struct" | wc -c , echo "AAAAAA" , echo "BBBB" , echo "CCCC" , echo "World"
// ( A | B , C | D | E , F , G | H )
// (ls -l | grep "root", cat parser.h | grep "struct" | wc -c , cat parser.c | grep "main")
// (cat parser.h | grep "struct" | wc -c , cat parser.h | grep "struct" | wc -c , echo "AAAAAA")

// cat parser.h | (wc -c, wc -l)
// ( cat parser.h | grep "struct") | (wc -l, wc -c)
// (ls -l | tr /a-z/ /A-Z/ ; echo "Done." ; cat parser.h | grep "struct" | wc -c ; cat parser.h | grep "struct" | wc -c ; echo "AAAAAA")
// (ls -l , ls , ls , ls , ps aux | grep Z , cat parser.h | grep "struct" | wc -c , echo "AAAAAA" , echo "BBBB" , echo "CCCC" , echo "World")