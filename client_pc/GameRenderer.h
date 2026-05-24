#pragma once

#include <vector>
#include <string>

#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>

#include "GameClient.h"

class GameRenderer {
public:
    GameRenderer();
    void run();

private:
    void processEvents();
    void update();
    void render();

    // UI events
    void handleMenuEvent(const sf::Event& event);
    void handlePlayingEvent(const sf::Event& event);
    void handleGameOverClick(sf::Vector2f mouse);

    // Network callbacks
    void connectToServer();
    void handleServerMessage(const nlohmann::json& msg);

    // Drawing
    void drawMenu();
    void drawGame();
    void drawBoard();
    void drawPieces();
    void drawStatusTexts();
    void drawGameOver();
    void drawDisconnectButton();
    void drawBackToMenuButton();

    // Board helpers
    sf::Vector2f boardToPixel(int row, int col) const;
    bool pixelToBoard(int x, int y, int& row, int& col) const;
    bool isMyPiece(int piece) const;
    void clearSelection();
    void resetToMenu();

    // State
    enum class State { MENU, CONNECTING, WAITING, PLAYING, GAME_OVER, DISCONNECTED };
    State state_ = State::MENU;

    sf::RenderWindow window_;
    GameClient client_;

    // Board data
    std::vector<std::vector<int>> board_;
    std::string myColor_;
    std::string currentTurn_;
    bool gameOver_ = false;
    std::string winner_;
    std::vector<std::string> possibleMoves_;
    std::string captureForcedFrom_;

    // Selection
    int selRow_ = -1, selCol_ = -1;
    bool hasSelection_ = false;

    // Menu widgets
    sf::Font font_;
    sf::Text ipLabel_, portLabel_, statusText_;
    sf::Text gameStatusText_;   // "You are White"
    sf::Text turnText_;         // "Turn: White (Your turn)"
    sf::String ipInput_ = "";
    sf::String portInput_ = "";
    sf::RectangleShape connectButton_, ipBox_, portBox_;
    sf::Text connectButtonText_, ipText_, portText_;
    bool ipFocused_ = false, portFocused_ = false;

    // Disconnect button (during game)
    sf::RectangleShape disconnectButton_;
    sf::Text disconnectButtonText_;

    // Back to menu button (game over)
    sf::RectangleShape backToMenuButton_;
    sf::Text backToMenuButtonText_;

    // Cursor blink
    sf::RectangleShape ipCursor_, portCursor_;
    bool showCursor_ = true;
    sf::Clock cursorClock_;

    // Input limits
    static constexpr int MAX_IP_LENGTH = 15;
    static constexpr int MAX_PORT_LENGTH = 5;

    // Constants
    static constexpr int BOARD_SIZE = 8;
    static constexpr int CELL_SIZE = 80;
    static constexpr int BOARD_X = 100;
    static constexpr int BOARD_Y = 80;
};