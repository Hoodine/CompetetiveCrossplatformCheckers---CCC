#include "CheckersBoard.h"
#include <stdexcept>
#include <sstream>
#include <cctype>
#include <cmath>

CheckersBoard::CheckersBoard() {
    reset();
}

void CheckersBoard::reset() {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            board[r][c] = EMPTY;
        }
    }

    // Расстановка чёрных (сверху, строки 0-2)
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if ((r + c) % 2 == 1) { // только чёрные клетки
                board[r][c] = BLACK_MAN;
            }
        }
    }

    // Расстановка белых (снизу, строки 5-7)
    for (int r = 5; r < 8; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if ((r + c) % 2 == 1) {
                board[r][c] = WHITE_MAN;
            }
        }
    }
}

int CheckersBoard::rowFromChar(char digit) {
    if (digit < '1' || digit > '8')
        return -1;
    return SIZE - (digit - '0');
}

int CheckersBoard::colFromChar(char letter) {
    if (letter < 'a' || letter > 'h')
        return -1;
    return letter - 'a';
}

char CheckersBoard::rowToChar(int row) {
    return static_cast<char>('0' + (SIZE - row));
}

char CheckersBoard::colToChar(int col) {
    return static_cast<char>('a' + col);
}

bool CheckersBoard::isValidCoord(int row, int col) const {
    return row >= 0 && row < SIZE && col >= 0 && col < SIZE;
}

bool CheckersBoard::isOwnPiece(int row, int col, Cell playerColor) const {
    if (!isValidCoord(row, col)) return false;
    Cell cell = board[row][col];
    if (playerColor == WHITE_MAN) {
        return cell == WHITE_MAN || cell == WHITE_KING;
    }
    else {
        return cell == BLACK_MAN || cell == BLACK_KING;
    }
}

bool CheckersBoard::isEnemyPiece(int row, int col, Cell playerColor) const {
    if (!isValidCoord(row, col)) return false;
    Cell cell = board[row][col];
    if (playerColor == WHITE_MAN) {
        return cell == BLACK_MAN || cell == BLACK_KING;
    }
    else {
        return cell == WHITE_MAN || cell == WHITE_KING;
    }
}

CheckersBoard::Cell CheckersBoard::opponentColor(Cell color) {
    return (color == WHITE_MAN) ? BLACK_MAN : WHITE_MAN;
}

bool CheckersBoard::isValidManMove(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    // Простая шашка ходит только на 1 клетку по диагонали
    if (std::abs(rowDiff) != 1 || std::abs(colDiff) != 1)
        return false;

    // Направление: белые ходят вверх (row уменьшается), чёрные — вниз (row увеличивается)
    if (playerColor == WHITE_MAN && rowDiff != -1)
        return false;
    if (playerColor == BLACK_MAN && rowDiff != 1)
        return false;

    // Конечная клетка должна быть пустой
    return board[toRow][toCol] == EMPTY;
}

bool CheckersBoard::isValidManCapture(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (std::abs(rowDiff) != 2 || std::abs(colDiff) != 2)
        return false;

    // Белые — только вверх (rowDiff = -2)
    if (playerColor == WHITE_MAN && rowDiff != -2)
        return false;
    // Чёрные — только вниз (rowDiff = +2)
    if (playerColor == BLACK_MAN && rowDiff != 2)
        return false;

    if (board[toRow][toCol] != EMPTY)
        return false;

    int midRow = fromRow + rowDiff / 2;
    int midCol = fromCol + colDiff / 2;
    return isEnemyPiece(midRow, midCol, playerColor);
}

bool CheckersBoard::isValidKingMove(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    // Дамка ходит по диагонали на любое количество клеток
    if (std::abs(rowDiff) != std::abs(colDiff))
        return false;
    if (rowDiff == 0)
        return false;

    // Проверяем, что путь свободен
    int rowStep = (rowDiff > 0) ? 1 : -1;
    int colStep = (colDiff > 0) ? 1 : -1;
    int steps = std::abs(rowDiff);

    for (int i = 1; i <= steps; ++i) {
        int r = fromRow + i * rowStep;
        int c = fromCol + i * colStep;
        if (i < steps) {
            // Промежуточные клетки должны быть пусты
            if (board[r][c] != EMPTY)
                return false;
        }
        else {
            // Конечная клетка должна быть пуста
            if (board[r][c] != EMPTY)
                return false;
        }
    }
    return true;
}

bool CheckersBoard::isValidKingCapture(int fromRow, int fromCol, int toRow, int toCol, Cell playerColor) const {
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (std::abs(rowDiff) != std::abs(colDiff))
        return false;
    if (std::abs(rowDiff) < 2)
        return false;

    int rowStep = (rowDiff > 0) ? 1 : -1;
    int colStep = (colDiff > 0) ? 1 : -1;
    int steps = std::abs(rowDiff);

    // Конечная клетка должна быть пуста
    if (board[toRow][toCol] != EMPTY)
        return false;

    int enemyCount = 0;
    int enemyRow = -1, enemyCol = -1;

    for (int i = 1; i < steps; ++i) {
        int r = fromRow + i * rowStep;
        int c = fromCol + i * colStep;
        if (isEnemyPiece(r, c, playerColor)) {
            enemyCount++;
            enemyRow = r;
            enemyCol = c;
        }
        else if (board[r][c] != EMPTY) {
            // На пути встретили свою фигуру — нельзя
            return false;
        }
    }

    // Должна быть ровно одна вражеская фигура на пути
    if (enemyCount != 1)
        return false;

    // За вражеской фигурой должны быть только пустые клетки до конечной
    int afterEnemyRow = enemyRow + rowStep;
    int afterEnemyCol = enemyCol + colStep;
    int remainingSteps = steps - (std::abs(afterEnemyRow - fromRow));

    for (int i = 0; i < remainingSteps; ++i) {
        int r = afterEnemyRow + i * rowStep;
        int c = afterEnemyCol + i * colStep;
        if (r == toRow && c == toCol)
            break;
        if (board[r][c] != EMPTY)
            return false;
    }

    return true;
}

bool CheckersBoard::hasMandatoryCapture(Cell playerColor) const {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (!isOwnPiece(r, c, playerColor))
                continue;

            Cell piece = board[r][c];
            bool isKing = (piece == WHITE_KING || piece == BLACK_KING);

            if (isKing) {
                const int dirs[4][2] = { {-1,-1}, {-1,1}, {1,-1}, {1,1} };
                for (int d = 0; d < 4; ++d) {
                    int dr = dirs[d][0], dc = dirs[d][1];
                    for (int i = 1; i < SIZE; ++i) {
                        int tr = r + dr * i;
                        int tc = c + dc * i;
                        if (!isValidCoord(tr, tc)) break;
                        if (board[tr][tc] == EMPTY) continue;
                        if (isOwnPiece(tr, tc, playerColor)) break;
                        // Враг — проверяем клетку за ним
                        int jr = tr + dr;
                        int jc = tc + dc;
                        if (isValidCoord(jr, jc) && board[jr][jc] == EMPTY)
                            return true;
                        break;
                    }
                }
            }
            else {
                // ПРОСТАЯ ШАШКА — только вперёд
                int forward = (playerColor == WHITE_MAN) ? -1 : 1;
                for (int dc : {-1, 1}) {
                    int midR = r + forward;
                    int midC = c + dc;
                    int toR = r + 2 * forward;
                    int toC = c + 2 * dc;
                    if (isValidCoord(toR, toC) &&
                        isEnemyPiece(midR, midC, playerColor) &&
                        board[toR][toC] == EMPTY) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void CheckersBoard::executeMove(int fromRow, int fromCol, int toRow, int toCol) {
    // Сохраняем фигуру
    Cell piece = board[fromRow][fromCol];
    board[fromRow][fromCol] = EMPTY;

    // Если это взятие — удаляем среднюю фигуру
    int rowDiff = toRow - fromRow;
    int colDiff = toCol - fromCol;

    if (std::abs(rowDiff) == 2 && std::abs(colDiff) == 2) {
        // Простое взятие (простая шашка)
        int midRow = fromRow + rowDiff / 2;
        int midCol = fromCol + colDiff / 2;
        board[midRow][midCol] = EMPTY;
    }
    else if (std::abs(rowDiff) > 2) {
        // Взятие дамкой: ищем и удаляем вражескую фигуру на пути
        int rowStep = (rowDiff > 0) ? 1 : -1;
        int colStep = (colDiff > 0) ? 1 : -1;
        for (int i = 1; i < std::abs(rowDiff); ++i) {
            int r = fromRow + i * rowStep;
            int c = fromCol + i * colStep;
            if (board[r][c] != EMPTY) {
                board[r][c] = EMPTY;
                break; // Удаляем только одну фигуру
            }
        }
    }

    // Ставим фигуру на новое место
    board[toRow][toCol] = piece;
}

void CheckersBoard::promoteIfNeeded(int row, int col) {
    Cell& cell = board[row][col];
    if (cell == WHITE_MAN && row == 0) {
        cell = WHITE_KING;
    }
    else if (cell == BLACK_MAN && row == SIZE - 1) {
        cell = BLACK_KING;
    }
}

bool CheckersBoard::makeMove(const std::string& from, const std::string& to, Cell playerColor) {
    if (from.size() != 2 || to.size() != 2)
        return false;

    int fromRow = rowFromChar(from[1]);
    int fromCol = colFromChar(from[0]);
    int toRow = rowFromChar(to[1]);
    int toCol = colFromChar(to[0]);

    if (!isValidCoord(fromRow, fromCol) || !isValidCoord(toRow, toCol))
        return false;

    if (!isOwnPiece(fromRow, fromCol, playerColor))
        return false;

    Cell piece = board[fromRow][fromCol];
    bool isKing = (piece == WHITE_KING || piece == BLACK_KING);

    // Проверяем, есть ли обязательное взятие на доске
    bool mandatory = hasMandatoryCapture(playerColor);
    bool thisPieceCanCapture = canCaptureFrom(from, playerColor);

    bool isCapture = false;
    bool isValid = false;

    if (isKing) {
        isValid = isValidKingMove(fromRow, fromCol, toRow, toCol, playerColor);
        if (!isValid) {
            isValid = isValidKingCapture(fromRow, fromCol, toRow, toCol, playerColor);
            isCapture = isValid;
        }
    }
    else {
        isValid = isValidManMove(fromRow, fromCol, toRow, toCol, playerColor);
        if (!isValid) {
            isValid = isValidManCapture(fromRow, fromCol, toRow, toCol, playerColor);
            isCapture = isValid;
        }
    }

    if (!isValid)
        return false;

    // Если есть обязательное взятие, то ход ДОЛЖЕН быть взятием
    if (mandatory && !isCapture) {
        return false; // Нельзя ходить тихо, когда есть взятие
    }

    // Если есть обязательное взятие, но эта фигура не может бить — нельзя ходить
    if (mandatory && !thisPieceCanCapture) {
        return false; // Другая фигура должна бить
    }

    // Выполняем ход
    executeMove(fromRow, fromCol, toRow, toCol);
    promoteIfNeeded(toRow, toCol);

    return true;
}

nlohmann::json CheckersBoard::toJson() const {
    nlohmann::json boardArray = nlohmann::json::array();
    for (int r = 0; r < SIZE; ++r) {
        nlohmann::json row = nlohmann::json::array();
        for (int c = 0; c < SIZE; ++c) {
            row.push_back(static_cast<int>(board[r][c]));
        }
        boardArray.push_back(row);
    }
    return boardArray;
}

bool CheckersBoard::isGameOver(Cell& winnerOut) const {
    int whiteCount = 0;
    int blackCount = 0;

    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (board[r][c] == WHITE_MAN || board[r][c] == WHITE_KING)
                whiteCount++;
            else if (board[r][c] == BLACK_MAN || board[r][c] == BLACK_KING)
                blackCount++;
        }
    }

    if (whiteCount == 0) {
        winnerOut = BLACK_MAN;
        return true;
    }
    if (blackCount == 0) {
        winnerOut = WHITE_MAN;
        return true;
    }

    winnerOut = EMPTY;
    return false;
}

std::string CheckersBoard::toString() const {
    std::ostringstream oss;
    oss << "  a b c d e f g h\n";
    for (int r = 0; r < SIZE; ++r) {
        oss << (SIZE - r) << " ";
        for (int c = 0; c < SIZE; ++c) {
            switch (board[r][c]) {
            case EMPTY:      oss << ((r + c) % 2 == 1 ? "." : " "); break;
            case WHITE_MAN:  oss << "w"; break;
            case BLACK_MAN:  oss << "b"; break;
            case WHITE_KING: oss << "W"; break;
            case BLACK_KING: oss << "B"; break;
            }
            oss << " ";
        }
        oss << (SIZE - r) << "\n";
    }
    oss << "  a b c d e f g h\n";
    return oss.str();
}

std::vector<std::string> CheckersBoard::getPossibleMoves(const std::string& from, Cell playerColor) const {
    std::vector<std::string> moves;
    std::vector<std::string> captures;

    int fromRow = rowFromChar(from[1]);
    int fromCol = colFromChar(from[0]);

    if (!isValidCoord(fromRow, fromCol)) return moves;
    if (!isOwnPiece(fromRow, fromCol, playerColor)) return moves;

    Cell piece = board[fromRow][fromCol];
    bool isKing = (piece == WHITE_KING || piece == BLACK_KING);

    // Проверяем, есть ли у ВСЕГО игрока обязательные взятия
    bool mandatory = hasMandatoryCapture(playerColor);
    // Проверяем, может ли ИМЕННО ЭТА фигура бить
    bool thisPieceCanCapture = canCaptureFrom(from, playerColor);

    if (isKing) {
        const int dirs[4][2] = { {-1,-1}, {-1,1}, {1,-1}, {1,1} };

        // Собираем взятия дамкой
        for (int d = 0; d < 4; ++d) {
            int dr = dirs[d][0], dc = dirs[d][1];
            for (int i = 1; i < SIZE; ++i) {
                int r = fromRow + dr * i;
                int c = fromCol + dc * i;
                if (!isValidCoord(r, c)) break;

                Cell target = board[r][c];
                if (target == EMPTY) continue;
                if (isOwnPiece(r, c, playerColor)) break;

                // Вражеская фигура — проверяем пустые клетки за ней
                for (int j = 1; j < SIZE; ++j) {
                    int jumpR = r + dr * j;
                    int jumpC = c + dc * j;
                    if (!isValidCoord(jumpR, jumpC)) break;
                    if (board[jumpR][jumpC] != EMPTY) break;

                    std::string to;
                    to += colToChar(jumpC);
                    to += rowToChar(jumpR);
                    captures.push_back(to);
                }
                break;
            }
        }

        // Тихие ходы дамкой
        for (int d = 0; d < 4; ++d) {
            int dr = dirs[d][0], dc = dirs[d][1];
            for (int i = 1; i < SIZE; ++i) {
                int r = fromRow + dr * i;
                int c = fromCol + dc * i;
                if (!isValidCoord(r, c) || board[r][c] != EMPTY) break;

                std::string to;
                to += colToChar(c);
                to += rowToChar(r);
                moves.push_back(to);
            }
        }
    }
    else {
        // Простая шашка: только вперёд
        int forward = (playerColor == WHITE_MAN) ? -1 : 1;

        // Взятия простой шашкой
        for (int dc : {-1, 1}) {
            int midR = fromRow + forward;
            int midC = fromCol + dc;
            int toR = fromRow + 2 * forward;
            int toC = fromCol + 2 * dc;

            if (isValidCoord(toR, toC) &&
                isEnemyPiece(midR, midC, playerColor) &&
                board[toR][toC] == EMPTY) {
                std::string to;
                to += colToChar(toC);
                to += rowToChar(toR);
                captures.push_back(to);
            }
        }

        // Тихие ходы простой шашкой
        for (int dc : {-1, 1}) {
            int r = fromRow + forward;
            int c = fromCol + dc;
            if (isValidCoord(r, c) && board[r][c] == EMPTY) {
                std::string to;
                to += colToChar(c);
                to += rowToChar(r);
                moves.push_back(to);
            }
        }
    }

    if (mandatory) {
        if (thisPieceCanCapture) {
            return captures; // Только взятия этой фигурой
        }
        else {
            return std::vector<std::string>(); // Пусто — этой фигурой нельзя ходить
        }
    }

    // Нет обязательных взятий — возвращаем всё
    captures.insert(captures.end(), moves.begin(), moves.end());
    return captures;
}

bool CheckersBoard::canCaptureFrom(const std::string& from, Cell playerColor) const {
    int row = rowFromChar(from[1]);
    int col = colFromChar(from[0]);

    if (!isValidCoord(row, col)) return false;
    if (!isOwnPiece(row, col, playerColor)) return false;

    Cell piece = board[row][col];
    bool isKing = (piece == WHITE_KING || piece == BLACK_KING);

    if (isKing) {
        // Дамка: все 4 направления
        const int dirs[4][2] = { {-1,-1}, {-1,1}, {1,-1}, {1,1} };
        for (int d = 0; d < 4; ++d) {
            int dr = dirs[d][0], dc = dirs[d][1];
            for (int i = 1; i < SIZE; ++i) {
                int r = row + dr * i;
                int c = col + dc * i;
                if (!isValidCoord(r, c)) break;

                Cell target = board[r][c];
                if (target == EMPTY) continue;
                if (isOwnPiece(r, c, playerColor)) break;

                // Вражеская фигура — проверяем пустую клетку за ней
                int jumpR = r + dr;
                int jumpC = c + dc;
                if (isValidCoord(jumpR, jumpC) && board[jumpR][jumpC] == EMPTY) {
                    return true;
                }
                break;
            }
        }
    }
    else {
        // ПРОСТАЯ ШАШКА: ТОЛЬКО ВПЕРЁД!
        // Белые ходят вверх (row уменьшается), чёрные — вниз (row увеличивается)
        int forward = (playerColor == WHITE_MAN) ? -1 : 1;

        // Проверяем только два диагональных взятия ВПЕРЁД
        for (int dc : {-1, 1}) {
            int midR = row + forward;      // клетка с врагом (на 1 вперёд)
            int midC = col + dc;
            int toR = row + 2 * forward;   // клетка приземления (на 2 вперёд)
            int toC = col + 2 * dc;

            if (isValidCoord(toR, toC) &&
                isEnemyPiece(midR, midC, playerColor) &&
                board[toR][toC] == EMPTY) {
                return true;
            }
        }
    }
    return false;
}

bool CheckersBoard::hasAnyMoves(Cell playerColor) const {
    for (int r = 0; r < SIZE; ++r) {
        for (int c = 0; c < SIZE; ++c) {
            if (isOwnPiece(r, c, playerColor)) {
                std::string from;
                from += colToChar(c);
                from += rowToChar(r);
                if (!getPossibleMoves(from, playerColor).empty()) {
                    return true;
                }
            }
        }
    }
    return false;
}