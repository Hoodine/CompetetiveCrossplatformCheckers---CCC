#include "TestClient.h"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  --ip <address>      Server IP address (default: 127.0.0.1)\n";
    std::cout << "  --port <port>       Server port (default: 5555)\n";
    std::cout << "  --delay <ms>        Startup delay in ms (default: 0)\n";
    std::cout << "                       Use for second client to start later\n";
    std::cout << "  --help, -h          Show this help\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << programName << "\n";
    std::cout << "  " << programName << " --delay 2000           (start 2 seconds later)\n";
    std::cout << "  " << programName << " --ip 192.168.1.100 --port 5555\n";
    std::cout << "\nTypical usage for two test clients:\n";
    std::cout << "  Terminal 1: test_client.exe\n";
    std::cout << "  Terminal 2: test_client.exe --delay 1000\n";
}

/*
int main(int argc, char* argv[]) {
    std::string serverIP = "127.0.0.1";
    int serverPort = 5555;
    int startupDelayMs = 0;

    // Обработка аргументов командной строки
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
        else if (arg == "--ip" && i + 1 < argc) {
            serverIP = argv[++i];
        }
        else if (arg == "--port" && i + 1 < argc) {
            serverPort = std::atoi(argv[++i]);
            if (serverPort <= 0 || serverPort > 65535) {
                std::cerr << "Invalid port number. Using default: 5555\n";
                serverPort = 5555;
            }
        }
        else if (arg == "--delay" && i + 1 < argc) {
            startupDelayMs = std::atoi(argv[++i]);
            if (startupDelayMs < 0) {
                startupDelayMs = 0;
            }
        }
        else {
            std::cerr << "Unknown argument: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    std::cout << "========================================\n";
    std::cout << "  Checkers Server Test Client\n";
    std::cout << "========================================\n";
    std::cout << "  Target: " << serverIP << ":" << serverPort << "\n";
    if (startupDelayMs > 0) {
        std::cout << "  Startup delay: " << startupDelayMs << " ms\n";
    }
    std::cout << "========================================\n";

    TestClient client(serverIP, serverPort, startupDelayMs);
    bool allPassed = client.runAllTests();

    // Возвращаем код ошибки для CI/CD
    return allPassed ? 0 : 1;
}
*/