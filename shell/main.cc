#include <cstdlib>
#include <iostream>
#include <string>
#include <stdio.h>
#include <vector>
#include <sstream>

void parse_and_run_command(const std::string &command) {
    /* TODO: Implement this. */

  std::istringstream s(command);
  std::vector<std::string> args;
  std::string token;
  while (s >> token) {
    args.push_back(token);
  }
  
    /* Note that this is not the correct way to test for the exit command.
       For example the command "   exit  " should also exit your shell.
     */
    if (command == "exit") {
        exit(0);
    }
    std::cerr << "Not implemented.\n";
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
