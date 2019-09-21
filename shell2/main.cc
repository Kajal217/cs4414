// Austin Baney | ab5ep | CS 4414 F19
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

typedef struct
{
    const char* path = 0;
    const char* args[50];
    const char* output = 0;
    const char* input = 0;
    pid_t pid = -1;
} command_t;

void parse_and_run_command(const std::string &command) {
    
    // PARSE INPUT TOKENS
    std::istringstream s(command);
    std::vector<std::string> tokens;
    std::string tkn;
    int cmdCount = 1;
    while (s >> tkn) {
        if (tkn == "|") {
            cmdCount++;
        }
        tokens.push_back(tkn);
    }

    // CREATE COMMAND PIPELINE
    std::vector<command_t> pipeline;
    uint tknIndex = 0;
    for (int j = 0; j < cmdCount; j++) {
        command_t cmd;
        memset(cmd.args, 0, sizeof(cmd.args));
        bool out = false, in = false;
        int argCount = 0;

        // create command object
        while (tknIndex < tokens.size()) {
            // if "|", create a new command
            if (tokens[tknIndex] == "|") {
                tknIndex++;
                break;
            }

            // output redirection
            if (out) {
                /*  command is malformed if it contains:
                        ">" followed by an operator 
                        or multiple output redirections     */
                if (tokens[tknIndex] == ">" || tokens[tknIndex] == "<" || tokens[tknIndex] == "|" || cmd.output != 0) {
                    std::cerr << "Invalid command\n";
                    return;
                }
                cmd.output = tokens[tknIndex].c_str();
                out = false;
                tknIndex++;
                continue;
            }
            if (tokens[tknIndex] == ">") {
                out = true;
                tknIndex++;
                continue;
            }

            // input redirection
            if (in) {
                /*  command is malformed if it contains:
                        "<" followed by an operator 
                        or multiple input redirections     */
                if (tokens[tknIndex] == ">" || tokens[tknIndex] == "<" || tokens[tknIndex] == "|" || cmd.input != 0) {
                    std::cerr << "Invalid command\n";
                    return;
                }
                cmd.input = tokens[tknIndex].c_str();
                in = false;
                tknIndex++;
                continue;
            }
            if (tokens[tknIndex] == "<") {
                in = true;
                tknIndex++;
                continue;
            }

            if (cmd.path == 0) cmd.path = tokens[tknIndex].c_str();
            cmd.args[argCount] = tokens[tknIndex].c_str();
            argCount++;
            tknIndex++;
        }
        cmd.args[argCount] = 0; // null terminate args

        // malformed if no path, or if redirecting to/from nothing
        if (cmd.path == 0 || out || in) {
            std::cerr << "Invalid command\n";
            return;
        }
        pipeline.push_back(cmd);
    }

    // pipe array
    int pipeFDs[25][2];
    int curReadFD = -1, curWriteFD = -1, prevReadFD = -1;

    // RUN COMMANDS
    const char* exitStr = "exit";
    // for each command in the line
    for (int i = 0; i < cmdCount; i++) {
        if (strcmp(pipeline[i].path, exitStr) == 0) {  // built-in exit command
            exit(0);
        }

        if (cmdCount > 1) {
            if (i < cmdCount - 1) {
                if (pipe(pipeFDs[i]) < 0) {
                    std::cerr << "Pipe failed\n";
                    return;
                }
                curReadFD = pipeFDs[i][0];
                curWriteFD = pipeFDs[i][1];
                printf("current read FD = %d\ni = %d\ncmdCount = %d\n", curReadFD, i, cmdCount);
            }
        }

        pid_t pid = fork();
        if (pid == 0) { // child process
            // printf("PATH: %s\nARGS[0]: %s\nARGS[1]: %s\nINPUT: %s\n", pipeline[i].path, pipeline[i].args[0], pipeline[i].args[1], pipeline[i].input);

            // connect pipe FDs
            if (cmdCount > 1) {
                if (i != 0) {
                    printf("previous read FD = %d\n", prevReadFD);
                    if (dup2(prevReadFD, 0) < 1) {
                        std::string err = "?";
                        if (errno == EBADF) err = "EBADF";
                        if (errno == EINTR) err = "EINTR";
                        if (errno == EMFILE) err = "EMFILE";
                        fprintf(stderr, "dup2() error: %d %s\n", errno, err.c_str());
                        std::cerr << "dup2() failed to connect previous pipe to stdin\n";
                    }
                    close(prevReadFD);
                }
                if (i < cmdCount - 1) {
                    if (dup2(curWriteFD, 1) < 1) {
                        std::cerr << "dup2() failed to connect current pipe to stdout\n";
                    }    
                }
                close(curReadFD);
                close(curWriteFD);
            }

            // output redirection
            if (pipeline[i].output != 0) {
                int outFD = open(pipeline[i].output, O_WRONLY | O_TRUNC | O_CREAT, 0666);
                if (outFD == -1) {
                    fprintf(stderr, "Open failed for: %s\n", pipeline[i].output);
                    exit(1);
                }
                dup2(outFD, 1);
                close(outFD);
            }

            // input redirection
            if (pipeline[i].input != 0) {
                int inFD = open(pipeline[i].input, O_RDONLY);
                if (inFD == -1) {
                    fprintf(stderr, "Open failed for: %s\n", pipeline[i].input);
                    exit(1);
                }
                dup2(inFD, 0);
                close(inFD);
            }

            // execute command
            execv(pipeline[i].path, (char**)pipeline[i].args);
            if (errno == ENOENT) std::cerr << "No such file or directory\n";
            fprintf(stderr, "Failed to execute command: %s\n", pipeline[i].path);
            exit(errno);

        } else if (pid > 0){ // parent process
            pipeline[i].pid = pid;
            // close pipes that future child processes won't need
            if (cmdCount > 1) {
                if (i != 0) {
                    close(prevReadFD);
                }
                if (i < cmdCount - 1) {
                    prevReadFD = curReadFD;
                    close(curWriteFD);
                }
            }
        } else { // fork failure
            std::cerr << "Fork failure\n";
            return;
        }
    }

    // close pipe FDs
    // if (cmdCount > 1) {
    //     for (int j = 0; j < cmdCount; j++) {
    //         close(pipeFDs[j][0]);
    //         close(pipeFDs[j][1]);
    //     }
    // }

    int status;
    // wait for each command to finish and check its status
    for (int j = 0; j < cmdCount; j++) {
        status = 0;
        waitpid(pipeline[j].pid, &status, 0);
        printf("%s exit status: %d\n", pipeline[j].path, WEXITSTATUS(status));
    }
}

int main(void) {
    std::string command;
    std::cout << "> ";
    while (std::getline(std::cin, command)) {
        parse_and_run_command(command);
        std::cout << "> ";
    }
    return 0;
}
