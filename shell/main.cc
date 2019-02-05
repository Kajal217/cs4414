#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <stdio.h>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

typedef struct
{
  const char* args[20];
  std::string name;
} command_t;

int isOperator(char* token) {
  if (strlen(token) > 1) return 0;
  if (*token == '>' || *token == '<') return 1;
  return 0;
}

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
  int numOps = 0;
  std::vector<const char*> opArr;
  command_t c;
  for (uint i=0; i < tokens.size(); i++) {
    if (tokens[i] == "&" && i != tokens.size()-1) {
	  std::cerr << "Invalid command.\n";
	  exit(1);
	}
    if (isOperator((char*)tokens[i].c_str()) == 1) {
	numOps++;
	opArr.push_back(tokens[i].c_str());
    }
    if (i==0){
      c.name = tokens[i].c_str();
      c.args[0] = "";
    }
    else c.args[i-1] = tokens[i].c_str();
  }

  //ERROR HANDLING

  // missing file
  if (c.name != "exit" && isOperator((char*)c.name.c_str()) == 0) {  
    DIR* dir = opendir(c.name.c_str());
    if (dir) {
      closedir(dir);
    }
    else if (ENOENT == errno) {
    std::cerr << "No such file or directory.\n";
    exit(1);
    }
  }

  //
  if (numOps > 2) {
    std::cerr << "Invalid command.\n";
    exit(1);
  }
  
  //for each command in the line
  std::vector<pid_t> pids;
  pid_t pid;
  int status;
  for (uint j = 0; j < 1; j++) {
    if (c.name == "exit") exit(0);
    pid = fork();
    if (pid==0) {
      //redirection stuff here*
      execv(c.name.c_str(), (char**)c.args);
      std::cerr << "exec failed, exiting...\n";
      exit(1);
    }
    else pids.push_back(pid);
  }
  //for each command in the line
  
  for (uint k=0; k<pids.size(); k++) {
    waitpid(pids[k], &status, 0);
    printf("%s exit status: %d.\n", c.name.c_str(), status/256);
  }
}

int main(void) {
    while (true) {
        std::string cmd;
        std::cout << "> ";
        std::getline(std::cin, cmd);
        parse_and_run_command(cmd);
    }
    return 0;
}
