#include "Server.h"
#include <iostream>

int main(int argc, char* argv[]) {
    int port = 5555;

    // Можно указать порт через аргумент командной строки
    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            std::cerr << "Invalid port. Using default: 5555\n";
            port = 5555;
        }
    }

    Server server(port);
    server.run();

    return 0;
}