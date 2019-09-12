#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <unistd.h>

typedef struct
{
    const char* path;
    const char* args[80];
} command_t;

void parse_and_run_command(const std::string &command) {
    /* TODO: Implement this. */
    /* Note that this is not the correct way to test for the exit command.
       For example the command "   exit  " should also exit your shell.
     */
    if (command == "exit") {
        exit(0);
    }
    
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
    int i = 0;
    for (std::string token : tokens) {
        if (i == 0) cmd.path = token;
        cmd.args[i] = token.c_str();
    }

    std::vector<pid_t> pids;
    // for each command in the line
    pid_t pid = fork();
    if (pid == 0) { // child process
        // do redirection stuff
        execv(cmd.path, (char**)&cmd.args[0]);
        // exec failed message
        exit(0);
    } else {
        pids.push_back(pid);
    }
    // end commands loop

    // for each command in the line
    // waitpid(stored pid, &status);
    // check return code in status;
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
