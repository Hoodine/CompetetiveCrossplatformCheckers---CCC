#pragma once

#include "NetworkCommon.h"
#include "lib/json.hpp"
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <cstdint>

/**
 * @brief Результат одного теста.
 */
struct TestResult {
    std::string name;
    bool passed;
    std::string errorMessage;
};

/**
 * @brief Тестовый клиент для проверки сервера шашек.
 *
 * Автоматически подключается к серверу, запускает серию тестов
 * и выводит результаты в консоль.
 */
class TestClient {
public:
    /**
     * @brief Конструктор.
     * @param serverIP IP-адрес сервера (например, "127.0.0.1")
     * @param serverPort Порт сервера (например, 5555)
     * @param startupDelayMs Задержка перед началом тестов в миллисекундах
     */
    TestClient(const std::string& serverIP, int serverPort, int startupDelayMs = 0);
    ~TestClient();

    /**
     * @brief Запустить все тесты.
     * @return true если все тесты пройдены.
     */
    bool runAllTests();

    /**
     * @brief Вывести сводку результатов.
     */
    void printSummary() const;

    /**
     * @brief Получить цвет игрока.
     */
    std::string getMyColor() const { return myColor_; }

    /**
     * @brief Получить ID комнаты.
     */
    int getRoomId() const { return roomId_; }

private:
    std::string serverIP_;
    int serverPort_;
    int startupDelayMs_;
    SocketType socket_;
    bool connected_;

    // Полученный цвет игрока
    std::string myColor_;
    int roomId_;

    std::vector<TestResult> results_;

    // Текущее состояние доски (для сравнения)
    nlohmann::json currentBoard_;
    bool boardReceived_;

    // Флаг: мы белые и должны ходить первыми
    bool isWhite_;
    bool myTurn_;

    // Счётчик ходов для отслеживания очерёдности
    int moveCount_;

    /**
     * @brief Подключение к серверу.
     */
    bool connect();

    /**
     * @brief Отключение от сервера.
     */
    void disconnect();

    /**
     * @brief Отправка сообщения серверу.
     * @param message JSON-объект для отправки.
     * @return true если отправка успешна.
     */
    bool sendMessage(const nlohmann::json& message);

    /**
     * @brief Получение сообщения от сервера.
     * @param timeoutMs Таймаут в миллисекундах (0 = бесконечно).
     * @param outMessage Выходной параметр для полученного JSON.
     * @return true если сообщение получено.
     */
    bool receiveMessage(int timeoutMs, nlohmann::json& outMessage);

    /**
     * @brief Ожидание сообщения определённого типа с повторными попытками.
     * @param expectedType Ожидаемый тип сообщения.
     * @param timeoutMs Таймаут одной попытки.
     * @param maxAttempts Максимальное количество попыток.
     * @param outMessage Выходной параметр.
     * @return true если получено сообщение нужного типа.
     */
    bool waitForMessage(const std::string& expectedType, int timeoutMs, int maxAttempts,
        nlohmann::json& outMessage);

    /**
     * @brief Ожидание game_start с увеличенным таймаутом.
     */
    bool waitForGameStart(int totalTimeoutMs);

    /**
     * @brief Ожидание board_state и обновление currentBoard_.
     */
    bool waitForBoardState(int timeoutMs, int maxAttempts = 3);

    /**
     * @brief Добавление результата теста.
     */
    void addResult(const std::string& testName, bool passed, const std::string& error = "");

    // ===================== Тесты =====================

    /**
     * @brief Тест 1: Подключение к серверу и получение game_start.
     */
    bool testConnection();

    /**
     * @brief Тест 2: Получение board_state после подключения.
     */
    bool testInitialBoardState();

    /**
     * @brief Тест 3: Проверка структуры JSON сообщений.
     */
    bool testMessageStructure();

    /**
     * @brief Тест 4: Проверка поля turn в board_state.
     */
    bool testTurnField();

    /**
     * @brief Тест 5: Проверка ping/pong.
     */
    bool testPingPong();

    /**
     * @brief Тест 6: Отправка сообщения с неизвестным типом.
     */
    bool testUnknownMessageType();

    /**
     * @brief Тест 7: Отправка валидного хода.
     */
    bool testValidMove();

    /**
     * @brief Тест 8: Отправка невалидного хода.
     */
    bool testInvalidMove();

    /**
     * @brief Тест 9: Отправка хода не в свою очередь.
     */
    bool testMoveOutOfTurn();
};