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
    const char* args[80];
    const char* output = 0;
    // const char* input = 0;
    pid_t pid = 0;
} command_t;

void err_invalid() {
    std::cerr << "Invalid command\n";
    exit(1);
}

void parse_and_run_command(const std::string &command) {
    
    // PARSE INPUT TOKENS
    std::istringstream s(command);
    std::vector<std::string> tokens;
    std::string tkn;
    while (s >> tkn) {
        tokens.push_back(tkn);
    }

    // CREATE COMMAND OBJECTS (checkpoint: only need 1)
    command_t cmd;
    memset(cmd.args, 0, sizeof(cmd.args));
    bool out = false;
    int argCount = 0;
    for (uint i = 0; i < tokens.size(); i++) {
        // output redirection
        if (out) {
            /*  command is malformed if it contains:
                    ">" followed by an operator 
                    or multiple output redirections     */
            if (tokens[i] == ">" || tokens[i] == "<" || tokens[i] == "|" || cmd.output != 0) {
                err_invalid();
            }
            cmd.output = tokens[i].c_str();
            out = false;
            continue;
        }
        if (tokens[i] == ">") {
            out = true;
            continue;
        }

        if (cmd.path == 0) cmd.path = tokens[i].c_str();
        cmd.args[argCount] = tokens[i].c_str();
        argCount++;
    }
    cmd.args[argCount] = 0;

    // malformed if no path, or if redirecting to nothing
    if (cmd.path == 0 || out) err_invalid();

    // RUN COMMANDS
    const char* exitStr = "exit";
    // for each command in the line
    if (strcmp(cmd.path, exitStr) == 0) {  // built-in exit command
        exit(0);
    }
    pid_t pid = fork();
    if (pid == 0) { // child process
        // printf("PATH: %s\nARGS[0]: %s\nARGS[1]: %s\nOUTPUT: %s\n", cmd.path, cmd.args[0], cmd.args[1], cmd.output);
        // output redirection
        if (cmd.output != 0) {
            int outFD = open(cmd.output, O_WRONLY | O_TRUNC | O_CREAT, 0666);
            if (outFD == -1) {
                fprintf(stderr, "Open failed for: %s\n", cmd.output);
                exit(1);
            }
            dup2(outFD, 1);
            close(outFD);
        }

        execv(cmd.path, (char**)cmd.args);
        if (errno == ENOENT) std::cerr << "No such file or directory\n";
        fprintf(stderr, "Failed to execute command: %s\n", cmd.path);
        exit(errno);
    } else if (pid > 0){ // parent process
        cmd.pid = pid;
    } else { // fork failure
        std::cerr << "Fork failure\n";
        std::cout << "> ";
        exit(1);
    }
    // end commands loop

    int status;
    // for each command in the line
    status = 0;
    waitpid(cmd.pid, &status, 0);
    printf("%s exit status: %d\n", cmd.path, WEXITSTATUS(status));
    // end commands loop
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
