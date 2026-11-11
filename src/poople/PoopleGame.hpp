#pragma once

#include "game/Game.hpp"
#include "poople/poople.hpp"
#include "utils/inputUtils.hpp"
#include "utils/utils.hpp"
#include <vector>

namespace Game {

class PoopleGame : public IGame {
private:
  Poople::Config getConfigFromUser();
  Poople::Config
  getConfigFromArgs(const std::map<std::string, std::string> &args);
  void printSolutions(const std::vector<Poople::Solution> &solutions,
                      const int limit = 100);

public:
  explicit PoopleGame();

  void runCLI() override;
  void runHeadless(const Utils::Input::CommandArgs &cmdArgs) override;
  std::string getGameName() const override { return "poople"; }
};

} // namespace Game
