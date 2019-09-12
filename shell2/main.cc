#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

typedef struct
{
    char* path = 0;
    const char* args[80];
    pid_t pid = 0;
} command_t;

void parse_and_run_command(const std::string &command) {
    
    // parse input into tokens
    std::istringstream s(command);
    std::vector<std::string> tokens;
    std::string tkn;
    while (s >> tkn) {
        tokens.push_back(tkn);
    }

    // create command objects from the tokens (checkpoint: only need 1)
    // std::vector<command_t> commands;
    command_t cmd;
    memset(cmd.args, 0, sizeof(cmd.args));
    for (int i = 0; i < tokens.size(); i++) {
        if (i == 0) cmd.path = tokens[i].c_str();
        else cmd.args[i] = tokens[i].c_str();
    }

    std::string exitStr = "exit";
    // for each command in the line
    if (strcmp(cmd.path, exitStr.c_str()) == 0) {  // built-in exit command
        exit(0);
    }
    pid_t pid = fork();
    if (pid == 0) { // child process
        // do redirection stuff
        if (execv(cmd.path, (char**)&(cmd.args[0])) < 0) {
            if (errno == ENOENT) std::cerr << "No such file or directory\n";
        }
        fprintf(stderr, "Failed to execute command: %s\n", cmd.path);
        exit(errno);
    } else {
        cmd.pid = pid;
    }
    // end commands loop

    int status;
    // for each command in the line
    status = 0;
    waitpid(cmd.pid, &status, 0);
    printf("%s exit status: %d\n", cmd.path, WEXITSTATUS(status));
    // end commands loop

    std::cerr << "Not implemented.\n";


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
