#include <stdlib.h>
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " PORT-NUMBER" << std::endl;
        return 1;
    }
    const char *portname = argv[1];

    std::cout << "Code to bind to 127.0.0.1 port " << portname
              << " and listen for connections unimplemented." << std::endl;
    return 0;
}
