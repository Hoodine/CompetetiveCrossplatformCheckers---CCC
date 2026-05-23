#pragma once

#include "NetworkCommon.h"
#include "CheckersBoard.h"
#include <string>
#include <atomic>
#include <mutex>
#include <vector>

class GameRoom {
public:
    GameRoom(SocketType playerWhite, SocketType playerBlack, int roomId);
    ~GameRoom();

    /**
     * @brief Запускает игровой цикл. Блокирует текущий поток.
     *        Должен вызываться в отдельном потоке.
     */
    void run();

    /**
     * @brief Возвращает true, если игра завершена.
     */
    bool isFinished() const { return finished_; }

    int getRoomId() const { return roomId_; }

private:
    SocketType players_[2];   // [0] = белые, [1] = чёрные
    int roomId_;
    CheckersBoard board_;
    std::atomic<bool> finished_{ false };
    bool mustCaptureAgain_ = false;
    std::string capturePiecePos_;

    /**
     * @brief Отправка JSON-сообщения одному игроку.
     */
    bool sendToPlayer(int playerIndex, const nlohmann::json& message);

    /**
     * @brief Отправка JSON-сообщения обоим игрокам.
     */
    void sendToBoth(const nlohmann::json& message);

    /**
     * @brief Получение JSON-сообщения от игрока.
     *        Блокируется до получения валидного JSON или отключения.
     * @return true если сообщение получено, false при отключении/ошибке.
     */
    bool receiveMessage(int playerIndex, nlohmann::json& outMessage);

    /**
     * @brief Отправка сообщения об ошибке игроку.
     */
    void sendError(int playerIndex, const std::string& errorText);

    /**
     * @brief Обработка отключения игрока.
     */
    void handleDisconnect(int disconnectedPlayerIndex);

    /**
     * @brief Отправка текущего состояния доски обоим игрокам.
     */
    void broadcastBoardState(int currentPlayerIndex);
};