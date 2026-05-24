#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "lib/json.hpp"

class CheckersBoard {
public:
    static const int SIZE = 8;

    enum Cell : int {
        EMPTY = 0,
        WHITE_MAN = 1,
        BLACK_MAN = 2,
        WHITE_KING = 3,
        BLACK_KING = 4
    };

    CheckersBoard();
    void reset();

    /**
     * @brief Попытка совершить ход.
     * @param from Начальная клетка в формате "a3", "b4" и т.д.
     * @param to   Конечная клетка.
     * @param player Цвет игрока: WHITE_MAN или BLACK_MAN.
     * @return true если ход легален и выполнен, false иначе.
     */
    bool makeMove(const std::string& from, const std::string& to, Cell playerColor);

    /**
     * @brief Сериализация доски в JSON (массив 8 строк по 8 чисел).
     */
    nlohmann::json toJson() const;

    /**
     * @brief Проверка окончания игры.
     * @param winnerOut Возвращает победителя (WHITE_MAN или BLACK_MAN) или EMPTY.
     * @return true если игра окончена.
     */
    bool isGameOver(Cell& winnerOut) const;

    /**
     * @brief Получить текстовое представление доски для консоли.
     */
    std::string toString() const;

    // Возвращает список допустимых конечных клеток для фигуры на 'from'
    // с учётом обязательности взятия и текущего цвета игрока
    std::vector<std::string> getPossibleMoves(const std::string& from, Cell playerColor) const;

    // Проверяет, есть ли у игрока playerColor хоть один допустимый ход
    bool hasAnyMoves(Cell playerColor) const;

    // Проверяет, может ли фигура на 'from' совершить взятие (для определения серии)
    bool canCaptureFrom(const std::string& from, Cell playerColor) const;

    bool isCaptureMove(const std::string& from, const std::string& to, Cell playerColor) const;

    static void setSimpleCanCaptureBackward(bool enable) { s_simpleCanCaptureBackward = enable; }
    static bool getSimpleCanCaptureBackward() { return s_simpleCanCaptureBackward; }

private:
    Cell board[SIZE][SIZE];

    static bool s_simpleCanCaptureBackward;

    // Преобразование шахматной нотации в координаты
    static int rowFromChar(char digit);  // '1'..'8' -> 7..0
    static int colFromChar(char letter); // 'a'..'h' -> 0..7
    static char rowToChar(int row);      // 7..0 -> '1'..'8'
    static char colToChar(int col);      // 0..7 -> 'a'..'h'

    bool isValidCoord(int row, int col) const;

    /**
     * @brief Проверяет, является ли клетка фигурой заданного цвета (включая дамок).
     */
    bool isOwnPiece(int row, int col, Cell playerColor) const;

    /**
     * @brief Проверяет, является ли клетка фигурой противника (включая дамок).
     */
    bool isEnemyPiece(int row, int col, Cell playerColor) const;

    /**
     * @brief Проверка конкретного хода (простая шашка).
     */
    bool isValidManMove(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const;

    /**
     * @brief Проверка взятия (простая шашка).
     */
    bool isValidManCapture(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const;

    /**
     * @brief Проверка хода дамкой.
     */
    bool isValidKingMove(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const;

    /**
     * @brief Проверка взятия дамкой.
     */
    bool isValidKingCapture(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const;

    /**
     * @brief Есть ли обязательное взятие для цвета playerColor?
     */
    bool hasMandatoryCapture(Cell playerColor) const;

    /**
     * @brief Получить цвет противника.
     */
    static Cell opponentColor(Cell color);

    /**
     * @brief Выполнить физическое перемещение/удаление фигур на доске.
     */
    void executeMove(int fromRow, int fromCol, int toRow, int toCol);

    /**
     * @brief Превратить шашку в дамку, если достигнут край.
     */
    void promoteIfNeeded(int row, int col);
};