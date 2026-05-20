#include "TestClient.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/time.h>
#endif

// ===================== Конструктор / Деструктор =====================

TestClient::TestClient(const std::string& serverIP, int serverPort, int startupDelayMs)
    : serverIP_(serverIP)
    , serverPort_(serverPort)
    , startupDelayMs_(startupDelayMs)
    , socket_(INVALID_SOCKET_VAL)
    , connected_(false)
    , roomId_(-1)
    , boardReceived_(false)
    , isWhite_(false)
    , myTurn_(false)
    , moveCount_(0)
{
}

TestClient::~TestClient() {
    disconnect();
}

// ===================== Сетевые методы =====================

bool TestClient::connect() {
    if (connected_) return true;

    initNetworking();

    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ == INVALID_SOCKET_VAL) {
        std::cerr << "[TestClient] Failed to create socket. Error: " << getSocketError() << "\n";
        return false;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(serverPort_));

    // Преобразование IP-адреса
    if (inet_pton(AF_INET, serverIP_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "[TestClient] Invalid IP address: " << serverIP_ << "\n";
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    std::cout << "[TestClient] Connecting to " << serverIP_ << ":" << serverPort_ << "...\n";

    if (::connect(socket_, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR_VAL) {
        std::cerr << "[TestClient] Connection failed. Error: " << getSocketError() << "\n";
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
        return false;
    }

    connected_ = true;
    std::cout << "[TestClient] Connected successfully!\n";
    return true;
}

void TestClient::disconnect() {
    if (socket_ != INVALID_SOCKET_VAL) {
        closeSocket(socket_);
        socket_ = INVALID_SOCKET_VAL;
    }
    connected_ = false;
    cleanupNetworking();
}

bool TestClient::sendMessage(const nlohmann::json& message) {
    if (!connected_) return false;

    std::string data = message.dump();
    uint32_t length = htonl(static_cast<uint32_t>(data.size()));

    int sent = send(socket_, reinterpret_cast<const char*>(&length), sizeof(length), 0);
    if (sent != sizeof(length)) {
        std::cerr << "[TestClient] Failed to send length. Error: " << getSocketError() << "\n";
        connected_ = false;
        return false;
    }

    sent = send(socket_, data.c_str(), static_cast<int>(data.size()), 0);
    if (sent != static_cast<int>(data.size())) {
        std::cerr << "[TestClient] Failed to send data. Error: " << getSocketError() << "\n";
        connected_ = false;
        return false;
    }

    return true;
}

bool TestClient::receiveMessage(int timeoutMs, nlohmann::json& outMessage) {
    if (!connected_) return false;

    // Если задан таймаут — используем select
    if (timeoutMs > 0) {
        fd_set readSet;
        FD_ZERO(&readSet);
        FD_SET(socket_, &readSet);

        timeval timeout;
        timeout.tv_sec = timeoutMs / 1000;
        timeout.tv_usec = (timeoutMs % 1000) * 1000;

        int selectResult = select(static_cast<int>(socket_) + 1, &readSet, nullptr, nullptr, &timeout);
        if (selectResult <= 0) {
            return false; // Таймаут или ошибка
        }
    }

    // Читаем длину
    uint32_t length = 0;
    int received = recv(socket_, reinterpret_cast<char*>(&length), sizeof(length), 0);
    if (received <= 0) {
        if (received == 0) {
            std::cerr << "[TestClient] Server closed connection.\n";
        }
        else {
            std::cerr << "[TestClient] recv error: " << getSocketError() << "\n";
        }
        connected_ = false;
        return false;
    }

    length = ntohl(length);

    if (length > 65536) {
        std::cerr << "[TestClient] Message too long: " << length << "\n";
        return false;
    }

    // Читаем сообщение
    std::vector<char> buffer(length + 1);
    int totalReceived = 0;
    while (totalReceived < static_cast<int>(length)) {
        received = recv(socket_, buffer.data() + totalReceived,
            static_cast<int>(length) - totalReceived, 0);
        if (received <= 0) {
            connected_ = false;
            return false;
        }
        totalReceived += received;
    }
    buffer[length] = '\0';

    try {
        outMessage = nlohmann::json::parse(buffer.data());
        return true;
    }
    catch (const nlohmann::json::parse_error& e) {
        std::cerr << "[TestClient] JSON parse error: " << e.what() << "\n";
        std::string preview(buffer.data(), min(size_t(200), length));
        std::cerr << "Raw data: " << preview << "\n";
        return false;
    }
}

bool TestClient::waitForMessage(const std::string& expectedType, int timeoutMs, int maxAttempts,
    nlohmann::json& outMessage) {
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        if (attempt > 1) {
            std::cout << "[TestClient] Retry " << attempt << "/" << maxAttempts
                << " waiting for '" << expectedType << "'...\n";
        }

        nlohmann::json msg;
        if (receiveMessage(timeoutMs, msg)) {
            std::string msgType = msg.value("type", "");

            if (msgType == expectedType) {
                outMessage = msg;
                return true;
            }

            // Если получили error — логируем, но продолжаем ждать
            if (msgType == "error") {
                std::cerr << "[TestClient] Received error while waiting for '" << expectedType
                    << "': " << msg.value("message", "unknown") << "\n";
            }

            // Если ждали game_start, а получили что-то другое — странно
            if (expectedType == "game_start") {
                std::cerr << "[TestClient] Unexpected message type '" << msgType
                    << "' while waiting for 'game_start'\n";
            }
        }
    }

    std::cerr << "[TestClient] Failed to receive '" << expectedType
        << "' after " << maxAttempts << " attempts\n";
    return false;
}

bool TestClient::waitForGameStart(int totalTimeoutMs) {
    std::cout << "[TestClient] Waiting for game_start (timeout: " << totalTimeoutMs << "ms)...\n";

    auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - startTime).count();

        if (elapsed >= totalTimeoutMs) {
            std::cerr << "[TestClient] Timeout waiting for game_start after " << totalTimeoutMs << "ms\n";
            std::cerr << "[TestClient] Make sure the second client is also connected to the server!\n";
            return false;
        }

        int remainingMs = static_cast<int>(totalTimeoutMs - elapsed);
        // Используем таймаут для одной попытки не более 2000 мс
        int singleTimeout = min(remainingMs, 2000);

        nlohmann::json msg;
        if (receiveMessage(singleTimeout, msg)) {
            std::string msgType = msg.value("type", "");
            if (msgType == "game_start") {
                myColor_ = msg.value("color", "");
                roomId_ = msg.value("room_id", -1);
                isWhite_ = (myColor_ == "white");
                std::cout << "[TestClient] Game started! Color: " << myColor_
                    << ", Room: " << roomId_ << "\n";
                return true;
            }
            else if (msgType == "error") {
                std::cerr << "[TestClient] Received error: " << msg.value("message", "") << "\n";
            }
        }
        // Если receiveMessage вернул false (таймаут) — продолжаем цикл
    }
}

bool TestClient::waitForBoardState(int timeoutMs, int maxAttempts) {
    nlohmann::json boardMsg;
    if (!waitForMessage("board_state", timeoutMs, maxAttempts, boardMsg)) {
        return false;
    }

    currentBoard_ = boardMsg;
    boardReceived_ = true;

    // Обновляем информацию о том, чей ход
    std::string turn = boardMsg.value("turn", "");
    myTurn_ = (turn == myColor_);

    return true;
}

// ===================== Результаты тестов =====================

void TestClient::addResult(const std::string& testName, bool passed, const std::string& error) {
    results_.push_back({ testName, passed, error });
    std::cout << "  [" << (passed ? "PASS" : "FAIL") << "] " << testName;
    if (!error.empty()) {
        std::cout << " - " << error;
    }
    std::cout << "\n";
}

// ===================== Запуск всех тестов =====================

bool TestClient::runAllTests() {
    std::cout << "\n========================================\n";
    std::cout << "  Checkers Server Test Suite\n";
    std::cout << "========================================\n\n";

    results_.clear();

    // Задержка перед стартом (чтобы второй клиент успел запуститься)
    if (startupDelayMs_ > 0) {
        std::cout << "[TestClient] Waiting " << startupDelayMs_ << "ms before connecting...\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(startupDelayMs_));
    }

    // Подключаемся
    if (!connect()) {
        addResult("Connection", false, "Failed to connect to server");
        printSummary();
        return false;
    }

    // Ждём game_start с большим таймаутом (30 секунд)
    if (!waitForGameStart(30000)) {
        addResult("Game Start", false, "Did not receive game_start. Is the second client connected?");
        printSummary();
        return false;
    }

    std::cout << "\n--- Running Tests ---\n\n";

    // Запускаем тесты
    testConnection();        // Тест 1
    testPingPong();          // Тест 5 (можно делать сразу)

    // Ждём первое состояние доски
    if (waitForBoardState(10000, 5)) {
        testInitialBoardState();   // Тест 2
        testMessageStructure();    // Тест 3
        testTurnField();           // Тест 4
    }
    else {
        addResult("Board State", false, "Failed to receive board_state");
    }

    testUnknownMessageType(); // Тест 6

    // Тесты ходов: выполняем по очереди
    // Белые ходят первыми
    if (isWhite_ && myTurn_) {
        // Обновляем состояние доски, если нужно
        if (!boardReceived_) {
            waitForBoardState(5000, 3);
        }

        testValidMove();      // Тест 7 (только белые)

        // Ждём обновления доски после хода
        waitForBoardState(5000, 3);

        testInvalidMove();    // Тест 8

        // Ждём ещё одно обновление
        waitForBoardState(5000, 3);
    }

    // Тест хода не в свою очередь (делают оба, когда не их очередь)
    // Ждём, пока будет не наша очередь
    if (boardReceived_ && myTurn_ == false) {
        testMoveOutOfTurn();  // Тест 9
    }
    else if (isWhite_ == false && boardReceived_ == false) {
        // Чёрные: ждём первого board_state
        waitForBoardState(10000, 5);
        if (myTurn_ == false) {
            testMoveOutOfTurn();
        }
    }

    std::cout << "\n--- Tests Complete ---\n";

    printSummary();

    // Проверяем, все ли тесты пройдены
    bool allPassed = true;
    for (const auto& result : results_) {
        if (!result.passed) {
            allPassed = false;
            break;
        }
    }

    return allPassed;
}

void TestClient::printSummary() const {
    std::cout << "\n========================================\n";
    std::cout << "  Test Summary";
    if (!myColor_.empty()) {
        std::cout << " (Player: " << myColor_ << ")";
    }
    std::cout << "\n========================================\n";

    int passed = 0;
    int failed = 0;

    for (const auto& result : results_) {
        if (result.passed) passed++;
        else failed++;
    }

    std::cout << "  Total:  " << results_.size() << "\n";
    std::cout << "  Passed: " << passed << "\n";
    std::cout << "  Failed: " << failed << "\n";

    if (failed > 0) {
        std::cout << "\n  Failed tests:\n";
        for (const auto& result : results_) {
            if (!result.passed) {
                std::cout << "    - " << result.name;
                if (!result.errorMessage.empty()) {
                    std::cout << ": " << result.errorMessage;
                }
                std::cout << "\n";
            }
        }
    }

    std::cout << "========================================\n\n";
}

// ===================== Реализация тестов =====================

bool TestClient::testConnection() {
    bool hasColor = !myColor_.empty();
    bool hasRoomId = (roomId_ >= 0);
    bool validColor = (myColor_ == "white" || myColor_ == "black");

    std::string errors;
    if (!hasColor) errors += "Missing color; ";
    if (!hasRoomId) errors += "Missing room_id; ";
    if (!validColor) errors += "Invalid color value '" + myColor_ + "'; ";

    addResult("Connection & Game Start", hasColor && hasRoomId && validColor, errors);
    return hasColor && hasRoomId && validColor;
}

bool TestClient::testInitialBoardState() {
    if (!boardReceived_) {
        addResult("Initial Board State", false, "No board_state received");
        return false;
    }

    auto boardMsg = currentBoard_;

    // Проверяем наличие обязательных полей
    if (!boardMsg.contains("board")) {
        addResult("Initial Board State", false, "Missing 'board' field");
        return false;
    }
    if (!boardMsg.contains("turn")) {
        addResult("Initial Board State", false, "Missing 'turn' field");
        return false;
    }

    // Проверяем размер доски
    auto board = boardMsg["board"];
    if (!board.is_array() || board.size() != 8) {
        addResult("Initial Board State", false, "Board is not 8x8 array");
        return false;
    }
    for (size_t i = 0; i < board.size(); ++i) {
        if (!board[i].is_array() || board[i].size() != 8) {
            addResult("Initial Board State", false, "Row " + std::to_string(i) + " is not size 8");
            return false;
        }
    }

    // Проверяем начальную расстановку
    bool correct = true;

    // Строки 0-2: чёрные шашки (значение 2)
    for (int r = 0; r < 3 && correct; ++r) {
        for (int c = 0; c < 8 && correct; ++c) {
            if ((r + c) % 2 == 1) {
                int cell = board[r][c];
                if (cell != 2) {
                    addResult("Initial Board State", false,
                        "Expected black man at (" + std::to_string(r) + "," + std::to_string(c) +
                        "), got " + std::to_string(cell));
                    correct = false;
                }
            }
        }
    }

    // Строки 3-4: пустые
    for (int r = 3; r < 5 && correct; ++r) {
        for (int c = 0; c < 8 && correct; ++c) {
            if (board[r][c] != 0) {
                addResult("Initial Board State", false,
                    "Expected empty at (" + std::to_string(r) + "," + std::to_string(c) +
                    "), got " + std::to_string(static_cast<int>(board[r][c])));
                correct = false;
            }
        }
    }

    // Строки 5-7: белые шашки (значение 1)
    for (int r = 5; r < 8 && correct; ++r) {
        for (int c = 0; c < 8 && correct; ++c) {
            if ((r + c) % 2 == 1) {
                int cell = board[r][c];
                if (cell != 1) {
                    addResult("Initial Board State", false,
                        "Expected white man at (" + std::to_string(r) + "," + std::to_string(c) +
                        "), got " + std::to_string(cell));
                    correct = false;
                }
            }
        }
    }

    if (correct) {
        addResult("Initial Board State", true);
    }
    return correct;
}

bool TestClient::testMessageStructure() {
    if (!boardReceived_) {
        addResult("Message Structure", false, "No board_state for analysis");
        return false;
    }

    bool structureOk = true;
    std::string errors;

    // Проверяем board_state
    if (!currentBoard_.contains("type")) { structureOk = false; errors += "Missing 'type'; "; }
    if (!currentBoard_.contains("board")) { structureOk = false; errors += "Missing 'board'; "; }
    if (!currentBoard_.contains("turn")) { structureOk = false; errors += "Missing 'turn'; "; }

    if (currentBoard_.contains("board") && !currentBoard_["board"].is_array()) {
        structureOk = false; errors += "'board' is not an array; ";
    }
    if (currentBoard_.contains("turn") && !currentBoard_["turn"].is_string()) {
        structureOk = false; errors += "'turn' is not a string; ";
    }
    if (currentBoard_.contains("type") && currentBoard_["type"] != "board_state") {
        structureOk = false; errors += "'type' is not 'board_state'; ";
    }

    addResult("Message Structure", structureOk, errors);
    return structureOk;
}

bool TestClient::testTurnField() {
    if (!boardReceived_) {
        addResult("Turn Field", false, "No board_state");
        return false;
    }

    std::string turn = currentBoard_.value("turn", "");
    if (turn != "white" && turn != "black") {
        addResult("Turn Field", false, "Invalid turn value: '" + turn + "'");
        return false;
    }

    addResult("Turn Field", true);
    return true;
}

bool TestClient::testPingPong() {
    nlohmann::json pingMsg = { {"type", "ping"} };
    if (!sendMessage(pingMsg)) {
        addResult("Ping/Pong", false, "Failed to send ping");
        return false;
    }

    nlohmann::json pongMsg;
    if (!waitForMessage("pong", 3000, 3, pongMsg)) {
        addResult("Ping/Pong", false, "Did not receive pong");
        return false;
    }

    addResult("Ping/Pong", true);
    return true;
}

bool TestClient::testUnknownMessageType() {
    nlohmann::json unknownMsg = {
        {"type", "strange_unknown_type"},
        {"data", "some random data"}
    };
    if (!sendMessage(unknownMsg)) {
        addResult("Unknown Message Type", false, "Failed to send message");
        return false;
    }

    nlohmann::json errorMsg;
    if (!waitForMessage("error", 3000, 3, errorMsg)) {
        addResult("Unknown Message Type", false, "No error received");
        return false;
    }

    std::string errorText = errorMsg.value("message", "");
    addResult("Unknown Message Type", true, "Server responded: " + errorText);
    return true;
}

bool TestClient::testValidMove() {
    if (!isWhite_) {
        addResult("Valid Move", true, "Skipped (not white)");
        return true;
    }
    if (!myTurn_) {
        addResult("Valid Move", true, "Skipped (not my turn)");
        return true;
    }

    // Ход белыми: a3 -> b4
    std::cout << "[TestClient] Attempting valid move: a3 -> b4\n";
    nlohmann::json moveMsg = {
        {"type", "move"},
        {"from", "a3"},
        {"to", "b4"}
    };
    if (!sendMessage(moveMsg)) {
        addResult("Valid Move", false, "Failed to send move");
        return false;
    }

    // Ждём board_state
    nlohmann::json boardMsg;
    if (!waitForMessage("board_state", 5000, 3, boardMsg)) {
        addResult("Valid Move", false, "No board_state after move");
        return false;
    }

    currentBoard_ = boardMsg;
    boardReceived_ = true;

    // Проверяем, что шашка переместилась
    auto board = boardMsg["board"];
    int fromCell = board[5][0]; // a3
    int toCell = board[4][1];   // b4

    if (fromCell != 0) {
        addResult("Valid Move", false, "Cell a3 should be empty, got " + std::to_string(fromCell));
        return false;
    }
    if (toCell != 1) {
        addResult("Valid Move", false, "Cell b4 should have white man, got " + std::to_string(toCell));
        return false;
    }

    // Обновляем myTurn_
    std::string turn = boardMsg.value("turn", "");
    myTurn_ = (turn == myColor_);
    moveCount_++;

    addResult("Valid Move", true);
    return true;
}

bool TestClient::testInvalidMove() {
    if (!isWhite_) {
        addResult("Invalid Move", true, "Skipped (not white)");
        return true;
    }

    // Пытаемся пойти назад: b4 -> a3
    std::cout << "[TestClient] Attempting invalid move: b4 -> a3\n";
    nlohmann::json moveMsg = {
        {"type", "move"},
        {"from", "b4"},
        {"to", "a3"}
    };
    if (!sendMessage(moveMsg)) {
        addResult("Invalid Move", false, "Failed to send move");
        return false;
    }

    // Ждём сообщение об ошибке
    nlohmann::json errorMsg;
    if (waitForMessage("error", 3000, 3, errorMsg)) {
        std::string errorText = errorMsg.value("message", "");
        addResult("Invalid Move", true, "Server rejected: " + errorText);
        return true;
    }

    // Если ошибки нет, возможно, сервер просто прислал board_state без изменений
    nlohmann::json boardMsg;
    if (waitForMessage("board_state", 3000, 1, boardMsg)) {
        currentBoard_ = boardMsg;
        boardReceived_ = true;
        // Проверяем, что b4 всё ещё наша шашка
        int cell = boardMsg["board"][4][1];
        if (cell == 1) {
            addResult("Invalid Move", true, "Move ignored (board unchanged)");
            return true;
        }
    }

    addResult("Invalid Move", false, "Unexpected response");
    return false;
}

bool TestClient::testMoveOutOfTurn() {
    // Отправляем ход, даже если не наша очередь
    std::cout << "[TestClient] Attempting move out of turn...\n";

    std::string from, to;
    if (isWhite_) {
        // Пытаемся пойти белой шашкой с b4 на c5
        from = "b4";
        to = "c5";
    }
    else {
        // Пытаемся пойти чёрной шашкой с b6 на a5
        from = "b6";
        to = "a5";
    }

    nlohmann::json moveMsg = {
        {"type", "move"},
        {"from", from},
        {"to", to}
    };
    if (!sendMessage(moveMsg)) {
        addResult("Move Out of Turn", false, "Failed to send message");
        return false;
    }

    // Ждём ответ
    nlohmann::json response;
    if (waitForMessage("error", 3000, 3, response)) {
        std::string errorText = response.value("message", "");
        addResult("Move Out of Turn", true, "Server rejected: " + errorText);
        return true;
    }

    // Может быть, сервер прислал board_state
    nlohmann::json boardMsg;
    if (waitForMessage("board_state", 3000, 1, boardMsg)) {
        currentBoard_ = boardMsg;
        boardReceived_ = true;
        addResult("Move Out of Turn", true, "Server sent board_state (move ignored)");
        return true;
    }

    addResult("Move Out of Turn", false, "No response from server");
    return false;
}