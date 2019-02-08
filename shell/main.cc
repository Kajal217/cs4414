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
#include <cstring>


typedef struct
{
  const char* args[20];
  int inIndex = -1;
  int outIndex = -1;
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

  
  //create commands
  
  int numOps = 0;
  command_t c;
  for (uint i=0; i < tokens.size(); i++) {
    if (tokens[i] == "&" && i != tokens.size()-1) {
	  std::cerr << "Invalid command.\n";
	  exit(1);
	}
    if (isOperator((char*)tokens[i].c_str()) == 1) {
	numOps++;
	if (tokens[i] == "<") c.inIndex = i;
	else if (tokens[i] == ">") c.outIndex = i;
    }
    c.args[i] = tokens[i].c_str();
    // if "|" start a new cmd object...
  }

  std::string ex = "exit";
  //BUILT-IN COMMAND
  if (strcmp(c.args[0],ex.c_str())==0) exit(0);
  
  //ERROR HANDLING

  // missing file
  if (isOperator((char*)c.args[0]) == 0) {  
    DIR* dir = opendir(c.args[0]);
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
    
    pid = fork();
    if (pid==0) {
      //redirection stuff here*
      execv(c.args[0], (char**)(&(c.args[0])));
      std::cerr << "exec failed, exiting...\n";
      exit(1);
    }
    else pids.push_back(pid);
  }
  //for each command in the line
  
  for (uint k=0; k<pids.size(); k++) {
    waitpid(pids[k], &status, 0);
    printf("%s exit status: %d.\n", c.args[0], WEXITSTATUS(status));
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
