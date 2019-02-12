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
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <stdlib.h>

//HANDLE REDIRECT AT BEGINNING OF COMMAND

typedef struct
{
  const char* args[30];
  int inIndex = -1;
  int outIndex = -1;
  int argSize = 0;
} command_t;

int isOperator(char* token) {
  if (strlen(token) > 1) return 0;
  if (*token == '>' || *token == '<') return 1;
  return 0;
}

void redirect(std::string infile, std::string outfile, command_t c){
  int in, out;
  std::vector<char*> out_args;
  //std::vector<char*> temp_args;
  //temp_args.push_back("echo", "UVA", NULL};
  //char* temp_args[] = {"echo", "UVA", NULL};
  if(outfile == "bad" && infile == "bad")
  {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }
  else
  {
    if(infile != "bad")
    {
      in = open(infile.c_str(), O_RDONLY);
      dup2(in, 0);
      close(in);
    }
    if(outfile != "bad")
    { 
      out = open(outfile.c_str(), O_WRONLY | O_APPEND | O_TRUNC | O_CREAT, 0600);
      dup2(out, 1);
      close(out);
    }
    
    out_args.push_back((char*)c.args[0]);
    int z = 1;
    while (isOperator((char*)c.args[z]) == 0) {
      out_args.push_back((char*)c.args[z]);
    }
    execvp(out_args[0], (char**)(&(out_args[0])));
    exit(1);
  }
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
  int numIn = 0;
  int numOut = 0;
  command_t c;
  for (uint i=0; i < tokens.size(); i++)
  {
    if (tokens[i] == "&" && i != tokens.size()-1)
    {
	  std::cerr << "invalid command.";
	  std::cout << "> ";
	  exit(1);
    }
    if (isOperator((char*)tokens[i].c_str()) == 1)
      {
	numOps++;
	if (i==0) {
	  std::cerr << "invalid command.";
	  std::cout << "> ";
	  exit(1);
        }
	else if (tokens[i] == "<") {
	  c.inIndex = i;
	  numIn++;
	}
	else if (tokens[i] == ">") {
	  c.outIndex = i;
	  numOut++;
	}
    }
    c.args[i] = tokens[i].c_str();
    c.argSize += 1;
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

  //invalid command
  if ( (numOps > 2) || (numIn>0 && c.inIndex==c.argSize-1) || (numOut>0 && c.outIndex==c.argSize-1) || (numIn>0 && isOperator((char*)c.args[c.inIndex+1])==1) || (numOut>0 && isOperator((char*)c.args[c.outIndex+1])==1) )
  {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }
  
  //for each command in the line
  std::vector<pid_t> pids;
  pid_t pid;
  int status;
  std::string i_file = "bad";
  std::string o_file = "bad";

  //dummy loop change later for pipe
  for (uint j = 0; j < 1; j++) {
    
    pid = fork();
    if (pid==0) {
      
      //redirection stuff here
      if (numOps > 0) {
	/*if (isOperator((char*)c.args[c.outIndex+1]) || isOperator((char*)c.args[c.outIndex-1])){
	    std::cerr << "invalid command.\n";
	    }*/
	if (numOut==1){
	  o_file = c.args[c.outIndex + 1];
	}
	if (numIn==1){
	  i_file = c.args[c.inIndex + 1];
	}
	redirect(i_file, o_file, c);
      }

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
