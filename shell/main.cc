// Austin Baney // ab5ep

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

typedef struct
{
  const char* args[80];
  char* name = 0;

  int numOps = 0;
  int numIn = 0;
  int numOut = 0;
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
  if(outfile == "no" && infile == "no")
  {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }
  else
  {
    if(infile != "no")
    {
      in = open(infile.c_str(), O_RDONLY);
      dup2(in, 0);
      close(in);
    }
    if(outfile != "no")
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

  int pipeCount = 1;
  
  // parse the input
  std::istringstream s(cmd);
  std::vector<std::string> tokens;
  std::string token;
  while (s >> token) {
    tokens.push_back(token);
    if (token=="|") pipeCount++;
  }

  if (tokens[tokens.size()-1]=="|" || tokens[0]=="|") {
    std::cerr << "invalid command.";
    std::cout << "> ";
    exit(1);
  }
  
  //create commands
  std::vector<command_t> pipeline;
  uint i;
  uint tokenIndex = 0;

  for (int n=0; n<pipeCount; n++)
    {
      command_t c;
      memset(c.args, 0, sizeof(c.args));
      for (i=0; i+tokenIndex < tokens.size(); i++)
	{
	  if (tokens[i+tokenIndex] == "&")
	    {
	      if (i==tokens.size()-1){
          c.background = true;
          //set all cmds in pipeline to background
          for (int y=0; y<pipeCount-1; y++){
            pipeline[y].background = true;
          }
	      }
	      else {
          std::cerr << "invalid command.";
          std::cout << "> ";
          exit(1);
	      }
	    }
	  if (isOperator((char*)tokens[i+tokenIndex].c_str()) == 1)
	    {
	      c.numOps++;
	      if (tokens[i+tokenIndex] == "<") {
		      c.inIndex = i;
		      c.numIn++;
	      }
	      else if (tokens[i+tokenIndex] == ">") {
		      c.outIndex = i;
		      c.numOut++;
	      }
	    }
	  if (tokens[i+tokenIndex]=="|") {
	    i++;
	    break;
	  }
	  if (tokens[i+tokenIndex] != "&") {
	    c.args[i] = tokens[i+tokenIndex].c_str();
	    c.argSize++;
	  }
	  if (i==0) {
	    if (isOperator((char*)tokens[0+tokenIndex].c_str())==0) c.name = (char*)tokens[0+tokenIndex].c_str();
	  }
	  if (i==2 && c.name==0) {
	    if (isOperator((char*)tokens[2+tokenIndex].c_str())==0) c.name = (char*)tokens[2+tokenIndex].c_str();
	  }
	  if (i==4 && c.name==0) {
	    if (isOperator((char*)tokens[4+tokenIndex].c_str())==0) c.name = (char*)tokens[4+tokenIndex].c_str();
	  }
	  
	}//end i loop
      
      c.args[i] = 0; //null terminator
      tokenIndex += i;
      pipeline.push_back(c);
    }//end m loop

  // ERROR HANDLING FOR EACH COMMAND
  for (int p=0; p<pipeCount; p++)
    {
  
      //no command given
      if (pipeline[p].name==0) {
	std::cerr << "invalid command.";
	std::cout << "> ";
	exit(1);
      }

      //BUILT-IN COMMAND
      std::string ex = "exit";
      if (strcmp(pipeline[p].name,ex.c_str())==0) exit(0);

      // missing file
    
      DIR* dir = opendir(pipeline[p].name);
      if (dir) {
	closedir(dir);
      }
      else if (ENOENT == errno) {
	std::cerr << "No such file or directory.\n";
	exit(1);
      }

      //invalid command checks
      if ( (pipeline[p].numOps > 2) || (pipeline[p].numIn>0 && pipeline[p].inIndex==pipeline[p].argSize-1) || (pipeline[p].numOut>0 && pipeline[p].outIndex==pipeline[p].argSize-1) || (pipeline[p].numIn>0 && isOperator((char*)pipeline[p].args[pipeline[p].inIndex+1])==1) || (pipeline[p].numOut>0 && isOperator((char*)pipeline[p].args[pipeline[p].outIndex+1])==1) )
	{
	  std::cerr << "invalid command.";
	  std::cout << "> ";
	  exit(1);
	}
      
    }//end pipe loop
  
  //for each command in the line
  std::vector<pid_t> pids;
  pid_t pid;
  int pipes[40][2];

  for (int j = 0; j < pipeCount; j++) {
    std::string i_file = "no";
    std::string o_file = "no";
    if (pipeCount != 1) {
      if(pipe(pipes[j]) < 0) {
	std::cerr << "pipe error";
	exit(1);
      }
    }
    
    pid = fork();
    if (pid==0) {

      if (pipeCount != 1) {
        if (j!=pipeCount-1) dup2(pipes[j][1], STDOUT_FILENO);
        if (j!=0) dup2(pipes[j][0], STDIN_FILENO);
        close(pipes[j][0]);
        close(pipes[j][1]);
      }
      //redirection
      if (pipeline[j].numOps > 0){
	if (pipeline[j].numOut==1){
	  o_file = pipeline[j].args[pipeline[j].outIndex + 1];
	}
	if (pipeline[j].numIn==1){
	  i_file = pipeline[j].args[pipeline[j].inIndex + 1];
	}
	redirect(i_file, o_file, pipeline[j]);
      }

      //execute command
      execv(pipeline[j].name, (char**)(&(pipeline[j].args[0])));
      std::cerr << "exec failed, exiting...\n";
      exit(1);
    }
    else if (pid > 0) {
      pids.push_back(pid);
    }
    else {
      //fork failure
      std::cerr << "fork failure\n";
      std::cout << "> ";
      exit(1);
    }
  }

  // close all pipes
  if (pipeCount != 1){
    for (int a=0; a<pipeCount; a++) {
      close(pipes[a][0]);
      close(pipes[a][1]);
    }
  }
  
  int status;
  process_t temp;
  //for each command in the line
  for (int k=0; k<pipeCount; k++) {
    status = 0;
    if (!pipeline[k].background) waitpid(pids[k], &status, 0);
    else waitpid(pids[k], &status, WNOHANG);

    //if(background) // save status to vector;
    if (pipeline[k].background) {
      temp.name = pipeline[k].name;
      temp.status = status;
      bground.push_back(temp);
    }
    
    else {
      printf("%s exit status: %d\n", pipeline[k].name, WEXITSTATUS(status));
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
