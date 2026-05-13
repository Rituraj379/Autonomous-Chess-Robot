#ifndef GAME_HPP
#define GAME_HPP

#include <string>

#include "../engine/board/board.hpp"

enum class GameMode {
    Manual,
    Vision
};

class Game {
private:
    Board board;
    bool gameOver;
    GameMode mode;

    bool isValidMoveFormat(const std::string& move);
    bool isLegalMove(const std::string& move);
    bool isCaptureMove(const std::string& move);
    std::string getManualMove();
    std::string getVisionMove();

    void applyMove(const std::string& move);

    void handleHumanMove();
    void handleRobotMove();
    void printTurn();
    void stopOnRobotFailure(const std::string& context);

public:
    explicit Game(GameMode mode);
    ~Game();

    void start();
};

#endif
