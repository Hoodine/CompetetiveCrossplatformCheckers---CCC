#include "GameClient.h"
#include <iostream>

GameClient::GameClient() {}

GameClient::~GameClient() {
    disconnect();
}

bool GameClient::connect(const std::string& ip, int port) {
    if (connected_) disconnect();

    initNetworking();
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET_VAL) {
        std::cerr << "GameClient: socket failed (" << getSocketError() << ")\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) <= 0) {
        std::cerr << "GameClient: invalid IP address\n";
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    if (::connect(socket_, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR_VAL) {
        std::cerr << "GameClient: connect failed (" << getSocketError() << ")\n";
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    connected_ = true;
    return true;
}

void GameClient::disconnect() {
    stopReceiving();
    if (socket_ != INVALID_SOCKET_VAL) {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
    }
    connected_ = false;
}

void GameClient::startReceiving() {
    if (!connected_) return;
    receiving_ = true;
    receiveThread_ = std::thread(&GameClient::receiveLoop, this);
}

void GameClient::stopReceiving() {
    receiving_ = false;
    if (receiveThread_.joinable())
        receiveThread_.join();
}

bool GameClient::sendMessage(const nlohmann::json& msg) {
    if (!connected_) return false;
    std::string data = msg.dump();
    uint32_t len = htonl(static_cast<uint32_t>(data.size()));
    if (send(socket_, reinterpret_cast<const char*>(&len), sizeof(len), 0) != sizeof(len)) {
        std::cerr << "GameClient: send len error\n";
        return false;
    }
    if (send(socket_, data.c_str(), static_cast<int>(data.size()), 0) != static_cast<int>(data.size())) {
        std::cerr << "GameClient: send data error\n";
        return false;
    }
    return true;
}

bool GameClient::pollMessage(nlohmann::json& outMsg) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    if (messageQueue_.empty()) return false;
    outMsg = std::move(messageQueue_.front());
    messageQueue_.pop();
    return true;
}

void GameClient::receiveLoop() {
    while (receiving_ && connected_) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);
        timeval tv = { 0, 100000 }; // 100 ms
        int sel = select(static_cast<int>(socket_) + 1, &readSet, nullptr, nullptr, &tv);
        if (sel <= 0) continue;

        uint32_t len = 0;
        int rc = recv(socket_, reinterpret_cast<char*>(&len), sizeof(len), 0);
        if (rc <= 0) {
            connected_ = false;
            break;
        }
        len = ntohl(len);
        if (len > 65536) continue;

        std::vector<char> buf(len + 1);
        int total = 0;
        while (total < static_cast<int>(len)) {
            rc = recv(socket_, buf.data() + total, len - total, 0);
            if (rc <= 0) {
                connected_ = false;
                break;
            }
            total += rc;
        }
        if (!connected_) break;
        buf[len] = '\0';

        try {
            nlohmann::json msg = nlohmann::json::parse(buf.data());
            std::lock_guard<std::mutex> lock(queueMutex_);
            messageQueue_.push(std::move(msg));
        }
        catch (...) {
            // ignore invalid JSON
        }
    }
    receiving_ = false;
}