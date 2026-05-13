#include <iostream>

#include "game/game.hpp"

int main() {
  int modeChoice = 1;

  std::cout << "Select mode:\n";
  std::cout << "1. Manual\n";
  std::cout << "2. Vision\n";
  std::cout << "Choice: ";
  std::cin >> modeChoice;

  GameMode mode = (modeChoice == 2) ? GameMode::Vision : GameMode::Manual;

  Game game(mode);
  game.start();
  return 0;
}
