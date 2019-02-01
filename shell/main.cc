#include <cstdlib>
#include <iostream>
#include <string>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

class command {
 public:
  std::string cmd;
  std::vector<std::string> args;

  command() {
    cmd = "";
    args = std::vector<std::string>(0);
  }
  
  void addArg(std::string arg) {
    args.push_back(arg);
  }
  //return a c-style array of args
  const char* argArray() {
    const char* arr[10];
    for (uint i=0; i<args.size(); i++) {
      arr[i] = args[i].c_str;
    }
    return arr;
  }
};

void parse_and_run_command(const std::string &cmd) {
    /* TODO: Implement this. */
  // parse input
  std::istringstream s(cmd);
  std::vector<std::string> tokens;
  std::string token;
  while (s >> token) {
    tokens.push_back(token);
  }

  //create commands (deal with operators later)
  //pipeline object?
  command *cur = new command();
  for (uint i=0; i < tokens.size(); i++) {
    if (cur->cmd == "") cur->cmd = tokens[i];
    //if operator...
    else cur->addArg(tokens[i]);
  }

  //for each command in the line
  std::vector<int> pids;
  int status;
  for (uint j = 0; j < 1; j++) {
    pid_t pid = fork();
    if (pid==0) {
      //redirection stuff here*
      execv(cur->cmd.c_str(), (char**)cur->argArray());
      std::cerr << "exec failed, exiting...";
      exit(0);
    }
    else pids.push_back(pid);
  }
  //for each command in the line
  for (uint k=0; k<pids.size(); k++) {
    waitpid(pids[k], status*);
    printf("%s exited with status: %d", cur->cmd, status.c_str());
  }

}

int main(void) {
    while (true) {
        std::string command;
        std::cout << "> ";
        std::getline(std::cin, command);
        parse_and_run_command(command);
    }
    return 0;
}
