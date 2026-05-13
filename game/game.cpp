#include "game.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <vector>
#include <windows.h>

#include "../engine/board/board_to_fen.hpp"
#include "../engine/board/fen.hpp"
#include "../engine/move/makemove.hpp"
#include "../engine/movegen/legal.hpp"
#include "../engine/utils/move_to_string.hpp"
#include "chess_interface.hpp"
#include "robot_interface.hpp"

using namespace std;

namespace {

constexpr const char* kInitialFen =
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
constexpr const char* kVisionMoveFile = "game/vision_moves.txt";
constexpr DWORD kVisionPollIntervalMs = 300;

string trimCopy(const string& text) {
    size_t start = 0; 
    while (start < text.size() &&
           isspace(static_cast<unsigned char>(text[start]))) {
        ++start;
    }

    size_t end = text.size();
    while (end > start &&
           isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }

    return text.substr(start, end - start);
}

} // namespace

Game::Game(GameMode selectedMode) : gameOver(false), mode(selectedMode) {
    if (!parseFEN(kInitialFen, board)) {
        gameOver = true;
    }

    if (mode == GameMode::Vision) {
        ofstream ensureFile(kVisionMoveFile, ios::app);
    }
}

Game::~Game() {
    robotShutdown();
}

bool Game::isValidMoveFormat(const string& move) {
    if (move.size() != 4) {
        return false;
    }

    char f1 = move[0];
    char r1 = move[1];
    char f2 = move[2];
    char r2 = move[3];

    bool sourceOk =
        (f1 >= 'a' && f1 <= 'h') &&
        (r1 >= '1' && r1 <= '8');

    bool destOk =
        (f2 >= 'a' && f2 <= 'h') &&
        (r2 >= '1' && r2 <= '8');

    return sourceOk && destOk;
}

bool Game::isLegalMove(const string& move) {
    vector<Move> legalMoves = generateLegalMoves(board);

    for (const Move& m : legalMoves) {
        if (moveToString(m) == move) {
            return true;
        }
    }

    return false;
}

bool Game::isCaptureMove(const string& move) {
    vector<Move> legalMoves = generateLegalMoves(board);

    for (const Move& m : legalMoves) {
        if (moveToString(m) == move) {
            return m.flag == CAPTURE;
        }
    }

    return false;
}

void Game::applyMove(const string& move) {
    vector<Move> legalMoves = generateLegalMoves(board);

    for (const Move& m : legalMoves) {
        if (moveToString(m) == move) {
            Undo u;
            makeMove(board, m, u);
            return;
        }
    }
}

void Game::printTurn() {
    cout << "\n=========================\n";

    if (board.whiteToMove) {
        cout << "WHITE TO MOVE\n";
    } else {
        cout << "BLACK TO MOVE\n";
    }

    cout << "=========================\n";
}

void Game::stopOnRobotFailure(const string& context) {
    cout << "\n[ROBOT] Failure during " << context << ". Stopping game to avoid desync.\n";
    gameOver = true;
}

string Game::getManualMove() {
    string move;

    cout << "\nEnter move: ";
    cin >> move;
    return move;
}

string Game::getVisionMove() {
    while (true) {
        ifstream inFile(kVisionMoveFile);
        if (!inFile) {
            Sleep(kVisionPollIntervalMs);
            continue;
        }

        vector<string> remainingLines;
        string line;
        string move;
        bool foundMove = false;

        while (getline(inFile, line)) {
            if (!foundMove) {
                string trimmed = trimCopy(line);
                if (!trimmed.empty()) {
                    move = trimmed;
                    foundMove = true;
                    continue;
                }
            }

            remainingLines.push_back(line);
        }
        inFile.close();

        if (!foundMove) {
            Sleep(kVisionPollIntervalMs);
            continue;
        }

        ofstream outFile(kVisionMoveFile, ios::trunc);
        for (size_t i = 0; i < remainingLines.size(); ++i) {
            outFile << remainingLines[i];
            if (i + 1 < remainingLines.size()) {
                outFile << '\n';
            }
        }
        outFile.close();

        cout << "\n[VISION] Detected move: " << move << "\n";
        return move;
    }
}

void Game::handleHumanMove() {
    while (true) {
        string move =
            (mode == GameMode::Manual) ? getManualMove() : getVisionMove();

        if (!isValidMoveFormat(move)) {
            if (mode == GameMode::Vision) {
                cout << "\n[VISION] Invalid format ignored: " << move << "\n";
            } else {
                cout << "\nInvalid format\n";
            }
            continue;
        }

        if (!isLegalMove(move)) {
            if (mode == GameMode::Vision) {
                cout << "\n[VISION] Illegal move ignored: " << move << "\n";
            } else {
                cout << "\nIllegal move\n";
            }
            continue;
        }

        applyMove(move);

        cout << "\n[HUMAN] Played: " << move << "\n";
        break;
    }
}

void Game::handleRobotMove() {
    string fen = boardToFEN(board);

    cout << "\n[ENGINE] Thinking...\n";

    string engineMove = getEngineMove(fen);

    if (engineMove == "checkmate" ||
        engineMove == "stalemate" ||
        engineMove == "invalid") {
        cout << "\nGame Over: " << engineMove << "\n";
        gameOver = true;
        return;
    }

    cout << "\n[ENGINE] Best Move: " << engineMove << "\n";

    bool capture = isCaptureMove(engineMove);
    if (capture) {
        string dest = engineMove.substr(2, 2);
        if (!robotCapture(dest)) {
            stopOnRobotFailure("capture removal");
            return;
        }
    }

    if (!robotMove(engineMove)) {
        stopOnRobotFailure("piece movement");
        return;
    }

    applyMove(engineMove);
}

void Game::start() {
    cout << "\n===== CHESS ROBOT =====\n";
    cout << "\nHuman = White";
    cout << "\nRobot = Black\n";
    cout << "\nMode = " << (mode == GameMode::Manual ? "Manual" : "Vision") << "\n";
    if (mode == GameMode::Vision) {
        cout << "[VISION] Watching " << kVisionMoveFile << "\n";
    }
    if (!robotRest()) {
        stopOnRobotFailure("startup rest");
    }

    while (!gameOver) {
        printTurn();
        handleHumanMove();

        if (gameOver) {
            break;
        }

        printTurn();
        handleRobotMove();

        if (gameOver) {
            break;
        }

        if (!robotRest()) {
            stopOnRobotFailure("post-move rest");
            break;
        }
    }

    cout << "\n===== GAME ENDED =====\n";
}
