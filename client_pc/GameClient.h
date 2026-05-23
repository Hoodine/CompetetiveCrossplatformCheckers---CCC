#pragma once

#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

#include "NetworkCommon.h"

#include "lib/json.hpp"


class GameClient {
public:
    GameClient();
    ~GameClient();

    bool connect(const std::string& ip, int port);
    void disconnect();
    bool isConnected() const { return connected_; }

    void startReceiving();
    void stopReceiving();

    bool sendMessage(const nlohmann::json& msg);
    bool pollMessage(nlohmann::json& outMsg);

private:
    void receiveLoop();

    SocketType socket_ = INVALID_SOCKET_VAL;
    std::atomic<bool> connected_{ false };
    std::atomic<bool> receiving_{ false };

    std::queue<nlohmann::json> messageQueue_;
    std::mutex queueMutex_;
    std::thread receiveThread_;
};