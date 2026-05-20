#pragma once

#include "NetworkCommon.h"
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>

class GameRoom;

class Server {
public:
    Server(int port);
    ~Server();

    /**
     * @brief Запускает сервер. Блокирует вызывающий поток.
     */
    void run();

    /**
     * @brief Останавливает сервер.
     */
    void stop();

private:
    int port_;
    SocketType listenSocket_;
    std::atomic<bool> running_{ false };

    std::mutex waitingMutex_;
    std::vector<SocketType> waitingPlayers_;

    std::vector<std::shared_ptr<GameRoom>> activeRooms_;
    std::mutex roomsMutex_;

    /**
     * @brief Поток приёма новых подключений.
     */
    void acceptLoop();

    /**
     * @brief Поток для подбора пар (матчмейкинг).
     */
    void matchmakingLoop();

    /**
     * @brief Очистка завершённых игровых комнат.
     */
    void cleanupFinishedRooms();
};