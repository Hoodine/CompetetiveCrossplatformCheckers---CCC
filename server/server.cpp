#include "Server.h"
#include "GameRoom.h"
#include <iostream>
#include <sstream>
#include <exception>

Server::Server(int port)
    : port_(port)
    , listenSocket_(INVALID_SOCKET_VAL)
{
}

Server::~Server() {
    stop();
}

void Server::run() {
    try {
        initNetworking();

        listenSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (listenSocket_ == INVALID_SOCKET_VAL) {
            std::cerr << "Failed to create socket. Error: " << getSocketError() << "\n";
            cleanupNetworking();
            return;
        }

        int optval = 1;
        setsockopt(listenSocket_, SOL_SOCKET, SO_REUSEADDR,
            reinterpret_cast<const char*>(&optval), sizeof(optval));

        sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(static_cast<u_short>(port_));

        if (bind(listenSocket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR_VAL) {
            std::cerr << "Bind failed. Error: " << getSocketError() << "\n";
            closeSocket(listenSocket_);
            cleanupNetworking();
            return;
        }

        if (listen(listenSocket_, SOMAXCONN) == SOCKET_ERROR_VAL) {
            std::cerr << "Listen failed. Error: " << getSocketError() << "\n";
            closeSocket(listenSocket_);
            cleanupNetworking();
            return;
        }

        std::cout << "========================================\n";
        std::cout << "  Checkers Server started on port " << port_ << "\n";
        std::cout << "  Waiting for players...\n";
        std::cout << "========================================\n";

        running_ = true;

        // Запускаем потоки с защитой от исключений
        std::thread acceptThread(&Server::acceptLoop, this);
        std::thread matchmakingThread(&Server::matchmakingLoop, this);

        std::cout << "Type 'quit' to stop the server.\n";
        std::string command;
        while (running_) {
            std::getline(std::cin, command);
            if (command == "quit" || command == "exit" || command == "stop") {
                running_ = false;
                break;
            }
        }

        // Закрываем слушающий сокет, чтобы разблокировать accept
        closeSocket(listenSocket_);

        acceptThread.join();
        matchmakingThread.join();

        // Закрываем все активные комнаты
        {
            std::lock_guard<std::mutex> lock(roomsMutex_);
            activeRooms_.clear();
        }

        cleanupNetworking();
        std::cout << "Server stopped.\n";
    }
    catch (const std::exception& e) {
        std::cerr << "Server EXCEPTION: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Server UNKNOWN EXCEPTION\n";
    }
}

void Server::stop() {
    running_ = false;
    if (listenSocket_ != INVALID_SOCKET_VAL) {
        closeSocket(listenSocket_);
        listenSocket_ = INVALID_SOCKET_VAL;
    }
}

void Server::acceptLoop() {
    try {
        while (running_) {
            sockaddr_in clientAddr;
            int clientAddrLen = sizeof(clientAddr);

            SocketType clientSocket = accept(listenSocket_,
                reinterpret_cast<sockaddr*>(&clientAddr),
#ifdef _WIN32
                & clientAddrLen
#else
                reinterpret_cast<socklen_t*>(&clientAddrLen)
#endif
            );

            if (!running_) break;

            if (clientSocket == INVALID_SOCKET_VAL) {
                if (running_) {
                    std::cerr << "Accept failed: " << getSocketError() << "\n";
                }
                continue;
            }

            char clientIP[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIP, INET_ADDRSTRLEN);
            int clientPort = ntohs(clientAddr.sin_port);

            std::cout << "[Server] New connection from " << clientIP << ":" << clientPort << "\n";

            {
                std::lock_guard<std::mutex> lock(waitingMutex_);
                waitingPlayers_.push_back(clientSocket);
                std::cout << "[Server] Players in queue: " << waitingPlayers_.size() << "\n";
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Accept loop EXCEPTION: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Accept loop UNKNOWN EXCEPTION\n";
    }
}

void Server::matchmakingLoop() {
    try {
        while (running_) {
            {
                std::lock_guard<std::mutex> lock(waitingMutex_);
                if (waitingPlayers_.size() >= 2) {
                    SocketType playerWhite = waitingPlayers_[0];
                    SocketType playerBlack = waitingPlayers_[1];
                    waitingPlayers_.erase(waitingPlayers_.begin(), waitingPlayers_.begin() + 2);

                    static int nextRoomId = 1;
                    int roomId = nextRoomId++;

                    std::cout << "[Server] Creating room " << roomId << "...\n";

                    auto room = std::make_shared<GameRoom>(playerWhite, playerBlack, roomId);

                    {
                        std::lock_guard<std::mutex> roomLock(roomsMutex_);
                        activeRooms_.push_back(room);
                    }

                    // Запускаем комнату в отдельном потоке с защитой
                    std::thread roomThread([room, this]() {
                        try {
                            room->run();
                        }
                        catch (const std::exception& e) {
                            std::cerr << "Room thread EXCEPTION: " << e.what() << "\n";
                        }
                        catch (...) {
                            std::cerr << "Room thread UNKNOWN EXCEPTION\n";
                        }

                        // Удаляем комнату из списка
                        std::lock_guard<std::mutex> roomLock(roomsMutex_);
                        auto it = std::find(activeRooms_.begin(), activeRooms_.end(), room);
                        if (it != activeRooms_.end()) {
                            activeRooms_.erase(it);
                        }
                        std::cout << "[Server] Room " << room->getRoomId() << " cleaned up.\n";
                        });
                    roomThread.detach();
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Matchmaking loop EXCEPTION: " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "Matchmaking loop UNKNOWN EXCEPTION\n";
    }
}