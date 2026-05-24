#include "GameRenderer.h"
#include <iostream>
#include <sstream>
#include <SFML/Window/Clipboard.hpp>

#ifdef _WIN32
    #include <Windows.h>
#endif


GameRenderer::GameRenderer() {
    window_.create(sf::VideoMode(900, 800), "Checkers Client", sf::Style::Close);
    window_.setFramerateLimit(60);

    // Load font
    if (!font_.loadFromFile("arial.ttf")) {
#ifdef _WIN32
        if (!font_.loadFromFile("C:/Windows/Fonts/arial.ttf")) {
            std::cerr << "Failed to load arial.ttf\n";
        }
#endif
    }

    // --- Меню (IP/Port/Connect) ---
    ipLabel_.setFont(font_);
    ipLabel_.setString("Server IP:");
    ipLabel_.setCharacterSize(24);
    ipLabel_.setFillColor(sf::Color::White);
    ipLabel_.setPosition(50, 100);

    ipBox_.setSize(sf::Vector2f(200, 40));
    ipBox_.setFillColor(sf::Color(50, 50, 50));
    ipBox_.setOutlineColor(sf::Color::White);
    ipBox_.setOutlineThickness(2);
    ipBox_.setPosition(180, 95);

    ipText_.setFont(font_);
    ipText_.setCharacterSize(24);
    ipText_.setFillColor(sf::Color::White);
    ipText_.setPosition(190, 100);

    portLabel_.setFont(font_);
    portLabel_.setString("Port:");
    portLabel_.setCharacterSize(24);
    portLabel_.setFillColor(sf::Color::White);
    portLabel_.setPosition(50, 170);

    portBox_.setSize(sf::Vector2f(100, 40));
    portBox_.setFillColor(sf::Color(50, 50, 50));
    portBox_.setOutlineColor(sf::Color::White);
    portBox_.setOutlineThickness(2);
    portBox_.setPosition(180, 165);

    portText_.setFont(font_);
    portText_.setCharacterSize(24);
    portText_.setFillColor(sf::Color::White);
    portText_.setPosition(190, 170);

    connectButton_.setSize(sf::Vector2f(150, 50));
    connectButton_.setFillColor(sf::Color(0, 150, 0));
    connectButton_.setPosition(180, 240);
    connectButtonText_.setFont(font_);
    connectButtonText_.setString("Connect");
    connectButtonText_.setCharacterSize(24);
    connectButtonText_.setFillColor(sf::Color::White);
    connectButtonText_.setPosition(210, 250);

    statusText_.setFont(font_);
    statusText_.setCharacterSize(20);
    statusText_.setFillColor(sf::Color::Yellow);
    statusText_.setPosition(50, 320);

    // --- Статус игры (две строки) ---
    gameStatusText_.setFont(font_);
    gameStatusText_.setCharacterSize(24);
    gameStatusText_.setFillColor(sf::Color::White);
    gameStatusText_.setPosition(50, 10);

    turnText_.setFont(font_);
    turnText_.setCharacterSize(24);
    turnText_.setFillColor(sf::Color::White);
    turnText_.setPosition(50, 40);

    // --- Кнопка Disconnect (во время игры) ---
    disconnectButton_.setSize(sf::Vector2f(140, 40));
    disconnectButton_.setFillColor(sf::Color(150, 50, 50));
    disconnectButton_.setPosition(740, 10);
    disconnectButtonText_.setFont(font_);
    disconnectButtonText_.setString("Disconnect");
    disconnectButtonText_.setCharacterSize(20);
    disconnectButtonText_.setFillColor(sf::Color::White);
    disconnectButtonText_.setPosition(755, 15);

    // --- Кнопка Back to Menu (конец игры) ---
    backToMenuButton_.setSize(sf::Vector2f(200, 50));
    backToMenuButton_.setFillColor(sf::Color(50, 150, 50));
    backToMenuButton_.setPosition(350, 400);
    backToMenuButtonText_.setFont(font_);
    backToMenuButtonText_.setString("Back to Menu");
    backToMenuButtonText_.setCharacterSize(24);
    backToMenuButtonText_.setFillColor(sf::Color::White);
    backToMenuButtonText_.setPosition(380, 410);

    // --- Курсоры ввода ---
    ipCursor_.setSize(sf::Vector2f(2, 30));
    ipCursor_.setFillColor(sf::Color::White);
    ipCursor_.setPosition(0, 0); // будет динамически позиционироваться

    portCursor_.setSize(sf::Vector2f(2, 30));
    portCursor_.setFillColor(sf::Color::White);
    portCursor_.setPosition(0, 0);
}

void GameRenderer::run() {
    while (window_.isOpen()) {
        processEvents();
        update();
        render();
    }
}

void GameRenderer::processEvents() {
    sf::Event event;
    while (window_.pollEvent(event)) {
        if (event.type == sf::Event::Closed) {
            client_.disconnect();
            window_.close();
        }

        switch (state_) {
        case State::MENU:
            handleMenuEvent(event);
            break;
        case State::PLAYING:
        case State::GAME_OVER: // клики в игре (в том числе по кнопкам)
            if (event.type == sf::Event::MouseButtonPressed) {
                sf::Vector2f mouse = window_.mapPixelToCoords(
                    sf::Vector2i(event.mouseButton.x, event.mouseButton.y));
                if (state_ == State::GAME_OVER) {
                    handleGameOverClick(mouse);
                }
                else {
                    // Disconnect button?
                    if (disconnectButton_.getGlobalBounds().contains(mouse)) {
                        resetToMenu();
                        return;
                    }
                    // Игровое поле (ход)
                    handlePlayingEvent(event);
                }
            }
            break;
        default:
            break;
        }
    }
}

void GameRenderer::handleMenuEvent(const sf::Event& event) {
    if (event.type == sf::Event::MouseButtonPressed) {
        sf::Vector2f mouse = window_.mapPixelToCoords(sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

        if (ipBox_.getGlobalBounds().contains(mouse)) {
            ipFocused_ = true;
            portFocused_ = false;
            return;
        }
        if (portBox_.getGlobalBounds().contains(mouse)) {
            portFocused_ = true;
            ipFocused_ = false;
            return;
        }
        if (connectButton_.getGlobalBounds().contains(mouse)) {
            connectToServer();
            return;
        }
        ipFocused_ = false;
        portFocused_ = false;
    }

    // Ctrl+V
    if (event.type == sf::Event::KeyPressed && event.key.control && event.key.code == sf::Keyboard::V) {
        sf::String clipboard = sf::Clipboard::getString();
        if (ipFocused_) {
            // Ограничиваем длину IP
            if (clipboard.getSize() > MAX_IP_LENGTH)
                clipboard = clipboard.substring(0, MAX_IP_LENGTH);
            ipInput_ = clipboard;
            ipText_.setString(ipInput_);
        }
        else if (portFocused_) {
            sf::String filtered;
            for (size_t i = 0; i < clipboard.getSize() && filtered.getSize() < MAX_PORT_LENGTH; ++i) {
                wchar_t c = clipboard[i];
                if (c >= L'0' && c <= L'9')
                    filtered += c;
            }
            portInput_ = filtered;
            portText_.setString(portInput_);
        }
        return;
    }

    // Backspace и обычный ввод
    if (event.type == sf::Event::TextEntered) {
        uint32_t unicode = event.text.unicode;
        if (unicode == 8) { // Backspace
            if (ipFocused_ && !ipInput_.isEmpty()) {
                ipInput_.erase(ipInput_.getSize() - 1);
                ipText_.setString(ipInput_);
            }
            else if (portFocused_ && !portInput_.isEmpty()) {
                portInput_.erase(portInput_.getSize() - 1);
                portText_.setString(portInput_);
            }
            return;
        }
        if (unicode < 32 || unicode == 127) return;

        if (ipFocused_) {
            if (ipInput_.getSize() < MAX_IP_LENGTH) {
                ipInput_ += static_cast<char>(unicode);
                ipText_.setString(ipInput_);
            }
        }
        else if (portFocused_) {
            if (unicode >= L'0' && unicode <= L'9' && portInput_.getSize() < MAX_PORT_LENGTH) {
                portInput_ += static_cast<char>(unicode);
                portText_.setString(portInput_);
            }
        }
    }
}

void GameRenderer::connectToServer() {
    int port = std::stoi(portInput_.toAnsiString());
    if (client_.connect(ipInput_.toAnsiString(), port)) {
        client_.startReceiving();
        state_ = State::WAITING;
        statusText_.setString("Waiting for opponent...");
    }
    else {
        statusText_.setString("Connection failed!");
    }
}

void GameRenderer::handlePlayingEvent(const sf::Event& event) {
    if (event.type != sf::Event::MouseButtonPressed ||
        event.mouseButton.button != sf::Mouse::Left)
        return;

    // Если не наш ход — игнорируем
    if (myColor_ != currentTurn_) {
        statusText_.setString("It's not your turn");
        return;
    }

    sf::Vector2f mouse = window_.mapPixelToCoords(
        sf::Vector2i(event.mouseButton.x, event.mouseButton.y));

    int row, col;
    if (!pixelToBoard(static_cast<int>(mouse.x), static_cast<int>(mouse.y), row, col))
        return;

    // Проверяем, кликнули ли на подсвеченную клетку (ход)
    if (hasSelection_ && !possibleMoves_.empty()) {
        std::string target;
        target += char('a' + col);
        target += char('8' - row);

        auto it = std::find(possibleMoves_.begin(), possibleMoves_.end(), target);
        if (it != possibleMoves_.end()) {
            std::string from;
            from += char('a' + selCol_);
            from += char('8' - selRow_);

            client_.sendMessage({
                {"type", "move"},
                {"from", from},
                {"to", target}
                });
            possibleMoves_.clear();
            hasSelection_ = false;
            statusText_.setString("");
            return;
        }
    }

    // Выделение своей шашки
    int piece = board_[row][col];

    if (!captureForcedFrom_.empty()) {
        std::string forcedPos = captureForcedFrom_;
        std::string clickedPos;
        clickedPos += char('a' + col);
        clickedPos += char('8' - row);
        if (forcedPos != clickedPos) {
            statusText_.setString("You must capture with the highlighted piece!");
            return;
        }
    }

    if (isMyPiece(piece)) {
        selRow_ = row;
        selCol_ = col;
        hasSelection_ = true;
        possibleMoves_.clear();

        std::string from;
        from += char('a' + col);
        from += char('8' - row);
        client_.sendMessage({ {"type", "get_moves"}, {"from", from} });
    }
    else {
        hasSelection_ = false;
        possibleMoves_.clear();
    }
}

void GameRenderer::update() {
    nlohmann::json msg;
    while (client_.pollMessage(msg)) {
        handleServerMessage(msg);
    }

    if (state_ == State::DISCONNECTED && !client_.isConnected()) {
        statusText_.setString("Disconnected from server.");
    }

    // Мигание курсора каждые 0.5 секунды
    if (cursorClock_.getElapsedTime().asSeconds() > 0.5f) {
        showCursor_ = !showCursor_;
        cursorClock_.restart();
    }
}

void GameRenderer::handleGameOverClick(sf::Vector2f mouse) {
    if (backToMenuButton_.getGlobalBounds().contains(mouse)) {
        resetToMenu();
    }
}

void GameRenderer::handleServerMessage(const nlohmann::json& msg) {
    std::string type = msg.value("type", "");

    if (type == "game_start") {
        myColor_ = msg["color"];
        state_ = State::PLAYING;
        gameStatusText_.setString("You are " + myColor_);
        turnText_.setString("");
        statusText_.setString("");
        possibleMoves_.clear();
        captureForcedFrom_.clear();
        hasSelection_ = false;
        board_.clear();
    }
    else if (type == "board_state") {
        board_.clear();
        for (const auto& row : msg["board"]) {
            std::vector<int> r;
            for (const auto& cell : row) r.push_back(cell.get<int>());
            board_.push_back(r);
        }
        currentTurn_ = msg.value("turn", "");

        // Обновляем текст хода
        std::string turnStr = "Turn: " + currentTurn_;
        if (myColor_ == currentTurn_) {
            turnStr += " (Your turn)";
        }
        else {
            turnStr += " (Opponent)";
        }
        turnText_.setString(turnStr);
        statusText_.setString("");

        // Принудительное взятие
        if (msg.contains("must_capture_from")) {
            captureForcedFrom_ = msg["must_capture_from"];
            std::string pos = captureForcedFrom_;
            selCol_ = pos[0] - 'a';
            selRow_ = '8' - pos[1];
            hasSelection_ = true;
            possibleMoves_.clear();
            client_.sendMessage({ {"type", "get_moves"}, {"from", pos} });
        }
        else {
            captureForcedFrom_.clear();
            if (myColor_ != currentTurn_) {
                hasSelection_ = false;
                possibleMoves_.clear();
            }
            else {
                hasSelection_ = false;
                possibleMoves_.clear();
                captureForcedFrom_.clear();
            }
        }
    }
    else if (type == "possible_moves") {
        possibleMoves_ = msg.value("moves", std::vector<std::string>());
    }
    else if (type == "error") {
        statusText_.setString("Error: " + msg.value("message", ""));
        hasSelection_ = false;
        possibleMoves_.clear();
    }
    else if (type == "game_over") {
        winner_ = msg.value("winner", "");
        gameOver_ = true;
        state_ = State::GAME_OVER;
    }
}

void GameRenderer::render() {
    window_.clear(sf::Color(30, 30, 30));

    if (state_ == State::MENU || state_ == State::CONNECTING || state_ == State::WAITING) {
        drawMenu();
    }
    else if (state_ == State::PLAYING || state_ == State::GAME_OVER) {
        drawGame();
        drawDisconnectButton(); // доступна всегда в игре
        if (state_ == State::GAME_OVER) {
            drawGameOver();
            drawBackToMenuButton();
        }
    }

    window_.display();
}

void GameRenderer::drawMenu() {
    window_.draw(ipLabel_);
    window_.draw(ipBox_);
    window_.draw(ipText_);
    window_.draw(portLabel_);
    window_.draw(portBox_);
    window_.draw(portText_);
    window_.draw(connectButton_);
    window_.draw(connectButtonText_);
    window_.draw(statusText_);

    // Отрисовка курсоров в полях ввода
    if (showCursor_) {
        if (ipFocused_) {
            // Позиция курсора после последнего символа
            sf::FloatRect textBounds = ipText_.getGlobalBounds();
            float cursorX = textBounds.left + textBounds.width + 2;
            float cursorY = ipText_.getPosition().y + 2;
            ipCursor_.setPosition(cursorX, cursorY);
            window_.draw(ipCursor_);
        }
        else if (portFocused_) {
            sf::FloatRect textBounds = portText_.getGlobalBounds();
            float cursorX = textBounds.left + textBounds.width + 2;
            float cursorY = portText_.getPosition().y + 2;
            portCursor_.setPosition(cursorX, cursorY);
            window_.draw(portCursor_);
        }
    }
}

void GameRenderer::drawGame() {
    drawBoard();
    drawPieces();
    drawStatusTexts();

    // Подсветка возможных ходов
    if (hasSelection_ && !possibleMoves_.empty()) {
        sf::CircleShape hint(CELL_SIZE * 0.2f);
        hint.setFillColor(sf::Color(0, 255, 0, 150));
        for (const auto& to : possibleMoves_) {
            int col = to[0] - 'a';
            int row = '8' - to[1];
            sf::Vector2f center = boardToPixel(row, col) + sf::Vector2f(CELL_SIZE / 2.f, CELL_SIZE / 2.f);
            hint.setPosition(center - sf::Vector2f(hint.getRadius(), hint.getRadius()));
            window_.draw(hint);
        }
    }

    // Выделение
    if (hasSelection_) {
        sf::RectangleShape highlight(sf::Vector2f(CELL_SIZE, CELL_SIZE));
        highlight.setFillColor(sf::Color(255, 255, 0, 100));
        highlight.setPosition(boardToPixel(selRow_, selCol_));
        window_.draw(highlight);

        if (!captureForcedFrom_.empty()) {
            sf::RectangleShape forceHighlight(sf::Vector2f(CELL_SIZE, CELL_SIZE));
            forceHighlight.setFillColor(sf::Color(255, 0, 0, 80));
            forceHighlight.setOutlineColor(sf::Color::Red);
            forceHighlight.setOutlineThickness(3.f);
            forceHighlight.setPosition(boardToPixel(selRow_, selCol_));
            window_.draw(forceHighlight);
        }
    }
}

void GameRenderer::drawStatusTexts() {
    window_.draw(gameStatusText_);
    window_.draw(turnText_);
}

void GameRenderer::drawDisconnectButton() {
    window_.draw(disconnectButton_);
    window_.draw(disconnectButtonText_);
}

void GameRenderer::drawBackToMenuButton() {
    window_.draw(backToMenuButton_);
    window_.draw(backToMenuButtonText_);
}

void GameRenderer::drawGameOver() {
    sf::RectangleShape overlay(sf::Vector2f(window_.getSize().x, window_.getSize().y));
    overlay.setFillColor(sf::Color(0, 0, 0, 180));
    window_.draw(overlay);

    sf::Text winText;
    winText.setFont(font_);
    winText.setString("Game Over!\nWinner: " + winner_);
    winText.setCharacterSize(40);
    winText.setFillColor(sf::Color::White);
    winText.setStyle(sf::Text::Bold);
    sf::FloatRect bounds = winText.getLocalBounds();
    winText.setPosition((window_.getSize().x - bounds.width) / 2.f,
        (window_.getSize().y - bounds.height) / 2.f - 100);
    window_.draw(winText);
}

void GameRenderer::drawBoard() {
    sf::RectangleShape cell(sf::Vector2f(CELL_SIZE, CELL_SIZE));
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            cell.setPosition(boardToPixel(r, c));
            cell.setFillColor((r + c) % 2 == 0 ? sf::Color(240, 217, 181) : sf::Color(181, 136, 99));
            window_.draw(cell);
        }
    }
}

void GameRenderer::drawPieces() {
    for (int r = 0; r < BOARD_SIZE; ++r) {
        for (int c = 0; c < BOARD_SIZE; ++c) {
            int piece = board_[r][c];
            if (piece == 0) continue;

            sf::CircleShape circle(CELL_SIZE * 0.4f);
            circle.setOrigin(circle.getRadius(), circle.getRadius());
            sf::Vector2f pos = boardToPixel(r, c) + sf::Vector2f(CELL_SIZE / 2.f, CELL_SIZE / 2.f);
            circle.setPosition(pos);

            switch (piece) {
            case 1: circle.setFillColor(sf::Color::White); break;
            case 2: circle.setFillColor(sf::Color::Black); break;
            case 3: circle.setFillColor(sf::Color::White);
                circle.setOutlineColor(sf::Color::Yellow);
                circle.setOutlineThickness(3.f); break;
            case 4: circle.setFillColor(sf::Color::Black);
                circle.setOutlineColor(sf::Color::Yellow);
                circle.setOutlineThickness(3.f); break;
            }
            window_.draw(circle);
        }
    }
}

// Helper functions
sf::Vector2f GameRenderer::boardToPixel(int row, int col) const {
    return sf::Vector2f(BOARD_X + col * CELL_SIZE, BOARD_Y + row * CELL_SIZE);
}

bool GameRenderer::pixelToBoard(int x, int y, int& row, int& col) const {
    if (x < BOARD_X || x >= BOARD_X + CELL_SIZE * BOARD_SIZE ||
        y < BOARD_Y || y >= BOARD_Y + CELL_SIZE * BOARD_SIZE)
        return false;
    col = (x - BOARD_X) / CELL_SIZE;
    row = (y - BOARD_Y) / CELL_SIZE;
    return true;
}

bool GameRenderer::isMyPiece(int piece) const {
    if (myColor_ == "white") return piece == 1 || piece == 3;
    if (myColor_ == "black") return piece == 2 || piece == 4;
    return false;
}

void GameRenderer::clearSelection() {
    selRow_ = selCol_ = -1;
    hasSelection_ = false;
}

void GameRenderer::resetToMenu() {
    client_.disconnect();
    state_ = State::MENU;
    board_.clear();
    myColor_.clear();
    currentTurn_.clear();
    gameOver_ = false;
    winner_.clear();
    possibleMoves_.clear();
    captureForcedFrom_.clear();
    hasSelection_ = false;
    ipFocused_ = false;
    portFocused_ = false;
    statusText_.setString("");
    gameStatusText_.setString("");
    turnText_.setString("");
    // Не сбрасываем ipInput_ и portInput_, чтобы можно было переподключиться к тому же адресу
}