#include "GameRoom.h"
#include <iostream>
#include <sstream>
#include <exception>

GameRoom::GameRoom(SocketType playerWhite, SocketType playerBlack, int roomId)
    : roomId_(roomId)
{
    players_[0] = playerWhite;
    players_[1] = playerBlack;
    std::cout << "[Room " << roomId_ << "] Created. Player sockets: "
        << players_[0] << " (white), " << players_[1] << " (black)\n";
}

GameRoom::~GameRoom() {
    std::cout << "[Room " << roomId_ << "] Destructor called\n";
    for (int i = 0; i < 2; ++i) {
        if (players_[i] != INVALID_SOCKET_VAL) {
            closeSocket(players_[i]);
            players_[i] = INVALID_SOCKET_VAL;
        }
    }
}

bool GameRoom::sendToPlayer(int playerIndex, const nlohmann::json& message) {
    if (players_[playerIndex] == INVALID_SOCKET_VAL) {
        std::cerr << "[Room " << roomId_ << "] ERROR: sendToPlayer called with invalid socket for player " << playerIndex << "\n";
        return false;
    }

    std::string data;
    try {
        data = message.dump();
    }
    catch (const std::exception& e) {
        std::cerr << "[Room " << roomId_ << "] ERROR: JSON dump failed: " << e.what() << "\n";
        return false;
    }

    uint32_t length = htonl(static_cast<uint32_t>(data.size()));

    int sent = send(players_[playerIndex], reinterpret_cast<const char*>(&length), sizeof(length), 0);
    if (sent != sizeof(length)) {
        std::cerr << "[Room " << roomId_ << "] ERROR: Failed to send length to player "
            << playerIndex << ". Error: " << getSocketError() << "\n";
        return false;
    }

    sent = send(players_[playerIndex], data.c_str(), static_cast<int>(data.size()), 0);
    if (sent != static_cast<int>(data.size())) {
        std::cerr << "[Room " << roomId_ << "] ERROR: Failed to send data to player "
            << playerIndex << ". Error: " << getSocketError() << "\n";
        return false;
    }

    std::cout << "[Room " << roomId_ << "] Sent to player " << playerIndex
        << ": " << data.substr(0, 80) << (data.size() > 80 ? "..." : "") << "\n";
    return true;
}

void GameRoom::sendToBoth(const nlohmann::json& message) {
    sendToPlayer(0, message);
    sendToPlayer(1, message);
}

bool GameRoom::receiveMessage(int playerIndex, nlohmann::json& outMessage) {
    if (players_[playerIndex] == INVALID_SOCKET_VAL) {
        std::cerr << "[Room " << roomId_ << "] ERROR: receiveMessage called with invalid socket\n";
        return false;
    }

    std::cout << "[Room " << roomId_ << "] Waiting for message from player " << playerIndex << "...\n";

    uint32_t length = 0;
    int received = recv(players_[playerIndex], reinterpret_cast<char*>(&length), sizeof(length), 0);
    if (received <= 0) {
        if (received == 0) {
            std::cout << "[Room " << roomId_ << "] Player " << playerIndex << " disconnected (EOF)\n";
        }
        else {
            std::cerr << "[Room " << roomId_ << "] recv length error: " << getSocketError() << "\n";
        }
        return false;
    }

    length = ntohl(length);
    if (length > 65536) {
        std::cerr << "[Room " << roomId_ << "] Message too long: " << length << "\n";
        return false;
    }

    std::vector<char> buffer(length + 1);
    int totalReceived = 0;
    while (totalReceived < static_cast<int>(length)) {
        received = recv(players_[playerIndex], buffer.data() + totalReceived,
            static_cast<int>(length) - totalReceived, 0);
        if (received <= 0) {
            std::cerr << "[Room " << roomId_ << "] recv data error: " << getSocketError() << "\n";
            return false;
        }
        totalReceived += received;
    }
    buffer[length] = '\0';

    try {
        outMessage = nlohmann::json::parse(buffer.data());
        std::cout << "[Room " << roomId_ << "] Received from player " << playerIndex
            << ": " << std::string(buffer.data()).substr(0, 80) << "\n";
        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[Room " << roomId_ << "] JSON parse error: " << e.what() << "\n";
        return false;
    }
}

void GameRoom::sendError(int playerIndex, const std::string& errorText) {
    nlohmann::json errMsg = {
        {"type", "error"},
        {"message", errorText}
    };
    sendToPlayer(playerIndex, errMsg);
}

void GameRoom::handleDisconnect(int disconnectedPlayerIndex) {
    int otherPlayer = 1 - disconnectedPlayerIndex;
    nlohmann::json winMsg = {
        {"type", "game_over"},
        {"reason", "opponent_disconnected"},
        {"winner", (otherPlayer == 0 ? "white" : "black")}
    };
    sendToPlayer(otherPlayer, winMsg);
    finished_ = true;
}

void GameRoom::broadcastBoardState(int currentPlayerIndex) {
    std::cout << "[Room " << roomId_ << "] Broadcasting board state (turn: "
        << (currentPlayerIndex == 0 ? "white" : "black") << ")\n";

    nlohmann::json boardMsg;
    try {
        boardMsg = {
            {"type", "board_state"},
            {"board", board_.toJson()},
            {"turn", (currentPlayerIndex == 0 ? "white" : "black")},
            {"board_string", board_.toString()}
        };

        // Если идёт серия взятий, добавляем обязательную фигуру
        if (mustCaptureAgain_) {
            boardMsg["must_capture_from"] = capturePiecePos_;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Room " << roomId_ << "] ERROR creating board message: " << e.what() << "\n";
        return;
    }
    sendToBoth(boardMsg);
}

void GameRoom::run() {
    std::cout << "[Room " << roomId_ << "] Game started.\n";

    try {
        // Отправляем начальные сообщения
        nlohmann::json startMsg0 = {
            {"type", "game_start"},
            {"color", "white"},
            {"room_id", roomId_}
        };
        nlohmann::json startMsg1 = {
            {"type", "game_start"},
            {"color", "black"},
            {"room_id", roomId_}
        };

        std::cout << "[Room " << roomId_ << "] Sending game_start to players...\n";
        sendToPlayer(0, startMsg0);
        sendToPlayer(1, startMsg1);

        int currentPlayer = 0; // 0 = белые
        mustCaptureAgain_ = false;

        // Отправляем ПЕРВОЕ состояние доски
        broadcastBoardState(currentPlayer);

        while (!finished_) {
            // Ждём сообщение от ЛЮБОГО игрока
            nlohmann::json moveMsg;
            int fromPlayer = -1;
            bool gotMessage = false;

            fd_set readSet;
            FD_ZERO(&readSet);
            FD_SET(players_[0], &readSet);
            FD_SET(players_[1], &readSet);

            timeval timeout;
            timeout.tv_sec = 1;
            timeout.tv_usec = 0;

            fd_set tmpSet = readSet;
            int maxSock = (players_[0] > players_[1] ? players_[0] : players_[1]) + 1;

            int selRet = select(maxSock, &tmpSet, nullptr, nullptr, &timeout);
            if (selRet > 0) {
                for (int i = 0; i < 2; ++i) {
                    if (FD_ISSET(players_[i], &tmpSet)) {
                        if (receiveMessage(i, moveMsg)) {
                            fromPlayer = i;
                            gotMessage = true;
                            break;
                        }
                        else {
                            handleDisconnect(i);
                            finished_ = true;
                            break;
                        }
                    }
                }
            }
            else if (selRet == 0) {
                continue;
            }
            else {
                std::cerr << "[Room " << roomId_ << "] select error: " << getSocketError() << "\n";
                break;
            }

            if (!gotMessage) continue;

            // Проверяем тип сообщения
            if (!moveMsg.contains("type")) {
                sendError(fromPlayer, "Missing 'type' field");
                continue;
            }

            std::string msgType = moveMsg["type"];
            std::cout << "[Room " << roomId_ << "] Message from player " << fromPlayer
                << " type: " << msgType << "\n";

            // ----- ОБЩИЕ сообщения (не зависят от очереди) -----
            if (msgType == "ping") {
                nlohmann::json pongMsg = { {"type", "pong"} };
                sendToPlayer(fromPlayer, pongMsg);
                continue;
            }

            if (msgType == "get_moves") {
                if (!moveMsg.contains("from")) {
                    sendError(fromPlayer, "Missing 'from' field");
                    continue;
                }
                std::string from = moveMsg["from"];
                CheckersBoard::Cell color = (fromPlayer == 0)
                    ? CheckersBoard::WHITE_MAN
                    : CheckersBoard::BLACK_MAN;

                std::vector<std::string> moves = board_.getPossibleMoves(from, color);

                if (mustCaptureAgain_ && from != capturePiecePos_) {
                    moves.clear();
                }

                nlohmann::json resp = {
                    {"type", "possible_moves"},
                    {"from", from},
                    {"moves", moves}
                };
                sendToPlayer(fromPlayer, resp);
                continue;
            }

            if (msgType == "resign") {
                std::string winnerStr = (fromPlayer == 0) ? "black" : "white";
                nlohmann::json gameOverMsg = {
                    {"type", "game_over"},
                    {"reason", "resignation"},
                    {"winner", winnerStr}
                };
                sendToBoth(gameOverMsg);
                finished_ = true;
                continue;
            }

            // ----- ХОДЫ: только от текущего игрока -----
            if (fromPlayer != currentPlayer) {
                sendError(fromPlayer, "It's not your turn");
                continue;
            }

            if (msgType == "move") {
                if (!moveMsg.contains("from") || !moveMsg.contains("to")) {
                    sendError(fromPlayer, "Missing 'from' or 'to' field");
                    continue;
                }

                std::string from = moveMsg["from"];
                std::string to = moveMsg["to"];

                if (mustCaptureAgain_ && from != capturePiecePos_) {
                    sendError(fromPlayer, "You must continue capturing with the same piece");
                    continue;
                }

                CheckersBoard::Cell playerColor = (currentPlayer == 0)
                    ? CheckersBoard::WHITE_MAN
                    : CheckersBoard::BLACK_MAN;

                if (board_.makeMove(from, to, playerColor)) {
                    std::cout << "[Room " << roomId_ << "] "
                        << (currentPlayer == 0 ? "White" : "Black")
                        << " moved: " << from << " -> " << to << "\n";
                    std::cout << board_.toString() << "\n";

                    // Проверяем победу (все шашки съедены)
                    CheckersBoard::Cell winner;
                    if (board_.isGameOver(winner)) {
                        std::string winnerStr = (winner == CheckersBoard::WHITE_MAN) ? "white" : "black";
                        nlohmann::json gameOverMsg = {
                            {"type", "game_over"},
                            {"reason", "all_pieces_captured"},
                            {"winner", winnerStr}
                        };
                        sendToBoth(gameOverMsg);
                        std::cout << "[Room " << roomId_ << "] Game over. Winner: " << winnerStr << "\n";
                        finished_ = true;
                        continue;
                    }

                    // Проверяем серию взятий
                    if (board_.canCaptureFrom(to, playerColor)) {
                        mustCaptureAgain_ = true;
                        capturePiecePos_ = to;
                        std::cout << "[Room " << roomId_ << "] Capture series continues from " << to << "\n";
                        // НЕ переключаем игрока, отправляем обновлённую доску
                        broadcastBoardState(currentPlayer);
                        continue;
                    }
                    else {
                        mustCaptureAgain_ = false;
                        // ПЕРЕКЛЮЧАЕМ игрока
                        currentPlayer = 1 - currentPlayer;

                        // Проверяем, может ли новый игрок ходить
                        CheckersBoard::Cell nextColor = (currentPlayer == 0)
                            ? CheckersBoard::WHITE_MAN
                            : CheckersBoard::BLACK_MAN;

                        if (!board_.hasAnyMoves(nextColor)) {
                            std::string winnerStr = (nextColor == CheckersBoard::WHITE_MAN) ? "black" : "white";
                            nlohmann::json gameOverMsg = {
                                {"type", "game_over"},
                                {"reason", "no_moves"},
                                {"winner", winnerStr}
                            };
                            sendToBoth(gameOverMsg);
                            std::cout << "[Room " << roomId_ << "] Game over (no moves). Winner: " << winnerStr << "\n";
                            finished_ = true;
                        }
                        else {
                            // Отправляем обновлённую доску ОБОИМ игрокам
                            broadcastBoardState(currentPlayer);
                        }
                    }
                }
                else {
                    sendError(fromPlayer, "Invalid move");
                    std::cout << "[Room " << roomId_ << "] Invalid move from player "
                        << fromPlayer << ": " << from << " -> " << to << "\n";
                }

            }
            else {
                sendError(fromPlayer, "Unknown message type: " + msgType);
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[Room " << roomId_ << "] EXCEPTION in run(): " << e.what() << "\n";
    }
    catch (...) {
        std::cerr << "[Room " << roomId_ << "] UNKNOWN EXCEPTION in run()\n";
    }

    // Закрываем сокеты
    for (int i = 0; i < 2; ++i) {
        if (players_[i] != INVALID_SOCKET_VAL) {
            closeSocket(players_[i]);
            players_[i] = INVALID_SOCKET_VAL;
        }
    }

    std::cout << "[Room " << roomId_ << "] Game finished.\n";
}