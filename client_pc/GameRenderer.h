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

    // Network callbacks
    void connectToServer();
    void handleServerMessage(const nlohmann::json& msg);

    // Drawing
    void drawMenu();
    void drawGame();
    void drawBoard();
    void drawPieces();
    void drawStatus(const std::string& text);
    void drawGameOver();

    // Board helpers
    sf::Vector2f boardToPixel(int row, int col) const;
    bool pixelToBoard(int x, int y, int& row, int& col) const;
    bool isMyPiece(int piece) const;
    void clearSelection();

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
    sf::Text ipLabel_, portLabel_, statusText_, gameStatusText_;
    sf::String ipInput_ = "127.0.0.1";
    sf::String portInput_ = "5555";
    sf::RectangleShape connectButton_, ipBox_, portBox_;
    sf::Text connectButtonText_, ipText_, portText_;
    bool ipFocused_ = false, portFocused_ = false;

    // Constants
    static constexpr int BOARD_SIZE = 8;
    static constexpr int CELL_SIZE = 80;
    static constexpr int BOARD_X = 100;
    static constexpr int BOARD_Y = 80;
};