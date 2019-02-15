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

// for all piped processwes stdin change to readin of prev pipe, stdout to write-out of current pipe

typedef struct
{
  const char* args[30];
  char* name = 0;
  int inIndex = -1;
  int outIndex = -1;
  int argSize = 0;
  bool background = false;
} command_t;

typedef struct
{
  std::string name = "";
  int status;
} process_t;

std::vector<process_t> bground;

int isOperator(char* token) {
  if (strlen(token) > 1) return 0;
  if (*token == '>' || *token == '<') return 1;
  return 0;
}

void redirect(std::string infile, std::string outfile, command_t c){
  int in, out;
  std::vector<char*> out_args;
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
    
    for (int z=0; z<c.argSize; z++) {
      if (isOperator((char*)c.args[z])==0) out_args.push_back((char*)c.args[z]);
      else z++;
    }
    out_args.push_back(0); //null terminate
    execvp((char*)c.name, (char**)(&(out_args[0])));
    exit(1);
  }
}

void parse_and_run_command(const std::string &cmd) {
  
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
  uint i;
  for (i=0; i < tokens.size(); i++)
  {
    if (tokens[i] == "&")
    {
      if (i==tokens.size()-1) c.background = true;
      else {
	std::cerr << "invalid command.";
	std::cout << "> ";
	exit(1);
      }
    }
    if (isOperator((char*)tokens[i].c_str()) == 1)
    {
	numOps++;
	if (tokens[i] == "<") {
	  c.inIndex = i;
	  numIn++;
	}
	else if (tokens[i] == ">") {
	  c.outIndex = i;
	  numOut++;
	}
    }
    if (tokens[i] != "&") c.args[i] = tokens[i].c_str();
    c.argSize += 1;
    if (i==0) {
      if (isOperator((char*)tokens[0].c_str())==0) c.name = (char*)tokens[0].c_str();
    }
    if (i==2 && c.name==0) {
      if (isOperator((char*)tokens[2].c_str())==0) c.name = (char*)tokens[2].c_str();
    }
    if (i==4 && c.name==0) {
      if (isOperator((char*)tokens[4].c_str())==0) c.name = (char*)tokens[4].c_str();
    }
    
    // if "|" put everything into a cmd object...
  }
  c.args[i] = 0; //null terminator

  //no command
  if (c.name==0) {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }

  //BUILT-IN COMMAND
  std::string ex = "exit";
  if (strcmp(c.name,ex.c_str())==0) exit(0);
  
  //ERROR HANDLING

  // missing file
  if (isOperator(c.name) == 0) {  
    DIR* dir = opendir(c.name);
    if (dir) {
      closedir(dir);
    }
    else if (ENOENT == errno) {
      std::cerr << "No such file or directory.\n";
      exit(1);
    }
  }

  //invalid command
  if ( (numOps > 2) || (numIn>0 && c.inIndex==c.argSize-1) || (numOut>0 && c.outIndex==c.argSize-1) || (numIn>0 && isOperator((char*)c.args[c.inIndex+1])==1) || (numOut>0 && isOperator((char*)c.args[c.outIndex+1])==1))
  {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }
  
  //for each command in the line
  std::vector<pid_t> pids;
  pid_t pid;
  //std::vector<bool> bg_pids;
  int status=0;
  std::string i_file = "bad";
  std::string o_file = "bad";

  //dummy loop change later for pipe
  for (uint j = 0; j < 1; j++) {
    
    pid = fork();
    if (pid==0) {
      
      //redirection stuff here
      if (numOps > 0){
	if (numOut==1){
	  o_file = c.args[c.outIndex + 1];
	}
	if (numIn==1){
	  i_file = c.args[c.inIndex + 1];
	}
	redirect(i_file, o_file, c);
      }

      execv(c.name, (char**)(&(c.args[0])));
      std::cerr << "exec failed, exiting...\n";
      exit(1);
    }
    else {
      pids.push_back(pid);
      /*
      if (c.background) bg_pids.push_back(true);
      else bg_pids.push_back(false);*/
    }
  }
  
  //save status into vector
  //print status when next non-background process finishes

  //structure for background processes, holds process name and status

  //background process finishes, move into vector
  // other process finishes, print status of bground processes in vector, empty it

  //should this vector be global???
  
  process_t temp;
  //for each command in the line
  for (uint k=0; k<pids.size(); k++) {
    if (!c.background) waitpid(pids[k], &status, 0);
    else waitpid(pids[k], &status, WNOHANG);

    //if(background) // save status to vector;
    if (c.background) {
      temp.name = c.name;
      temp.status = status;
      bground.push_back(temp);
    }
    
    else {
      printf("%s exit status: %d\n", c.name, WEXITSTATUS(status));
      // check the background vector and print finished processes
      if (!bground.empty()){
	for (uint m=0; m<bground.size(); m++) {
	  printf("%s exit status: %d\n", bground[m].name.c_str(), WEXITSTATUS(bground[m].status));
	}
      bground.clear();
      }
    }
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
