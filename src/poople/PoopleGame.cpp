#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "poople/PoopleGame.hpp"
#include "utils/inputUtils.hpp"

namespace Game {

PoopleGame::PoopleGame() {}

Poople::Config PoopleGame::getConfigFromUser() {
  Poople::Config config;

  std::string startWord = Utils::Input::promptLetters("Enter the start word:", 0, false);
  while (startWord.empty()) {
    std::cout << "Start word cannot be empty.\n";
    startWord = Utils::Input::promptLetters("Enter the start word:", 0, false);
  }
  config.startWord = startWord;

  std::string defaultEnd = (startWord.length() == 4) ? "poop" : "";
  std::string promptEndMsg = defaultEnd.empty()
                                 ? "Enter the target word (" + std::to_string(startWord.length()) + " letters):"
                                 : "Enter the target word [" + defaultEnd + "]:";

  std::string endWord = Utils::Input::promptLetters(promptEndMsg, static_cast<int>(startWord.length()), false);
  if (endWord.empty() && !defaultEnd.empty()) {
    endWord = defaultEnd;
  }
  while (endWord.length() != startWord.length()) {
    std::cout << "Target word must have the same length (" << startWord.length() << " letters).\n";
    endWord = Utils::Input::promptLetters(promptEndMsg, static_cast<int>(startWord.length()), false);
    if (endWord.empty() && !defaultEnd.empty()) {
      endWord = defaultEnd;
    }
  }
  config.endWord = endWord;

  std::cout << "\nSelect solver preset:\n"
            << "  1: Default (Find ALL solutions up to depth 10, common words)\n"
            << "  2: Fast (First 100 solutions up to depth 10)\n"
            << "  3: Exhaustive (All words including uncommon up to depth 10)\n"
            << "  0: Custom (Configure manually)\n";

  int preset = Utils::Input::promptInt("Enter preset number", 1, 0, 3);

  if (preset == 1) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = true;
    config.maxSolutions = 1000;
  } else if (preset == 2) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = true;
    config.maxSolutions = 100;
  } else if (preset == 3) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = false;
    config.maxSolutions = 1000;
  } else {
    config.minWords = static_cast<uint32_t>(
        Utils::Input::promptInt("Enter minimum ladder words", 2, 2, 20));
    config.maxWords = static_cast<uint32_t>(
        Utils::Input::promptInt("Enter maximum ladder words", 10, static_cast<int>(config.minWords), 20));
    config.excludeUncommonWords = Utils::Input::promptBool(
        "Exclude uncommon words? (y/n)", true);
    config.maxSolutions = static_cast<uint32_t>(
        Utils::Input::promptInt("Enter max solutions limit (0 for unlimited)", 1000, 0, 100000));
  }

  return config;
}

Poople::Config
PoopleGame::getConfigFromArgs(const std::map<std::string, std::string> &args) {
  Poople::Config config;

  config.startWord = Utils::Input::getArgValue(args, "start", std::string(""));
  if (config.startWord.empty()) {
    config.startWord = Utils::Input::getArgValue(args, "s", std::string(""));
  }

  config.endWord = Utils::Input::getArgValue(args, "end", std::string(""));
  if (config.endWord.empty()) {
    config.endWord = Utils::Input::getArgValue(args, "e", std::string(""));
  }

  // If start word is 4 letters and end is empty, default end to poop
  if (config.endWord.empty() && config.startWord.length() == 4) {
    config.endWord = "poop";
  }

  int preset = Utils::Input::getArgValue(args, "preset", 1);

  if (preset == 1) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = true;
    config.maxSolutions = 1000;
  } else if (preset == 2) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = true;
    config.maxSolutions = 100;
  } else if (preset == 3) {
    config.minWords = 2;
    config.maxWords = 10;
    config.excludeUncommonWords = false;
    config.maxSolutions = 1000;
  }

  // Override preset with specific args if passed
  config.minWords = static_cast<uint32_t>(
      Utils::Input::getArgValue(args, "min-words", static_cast<int>(config.minWords)));
  config.maxWords = static_cast<uint32_t>(
      Utils::Input::getArgValue(args, "max-words",
      Utils::Input::getArgValue(args, "max-depth", static_cast<int>(config.maxWords)))));
  config.excludeUncommonWords = Utils::Input::getArgValue(
      args, "exclude-uncommon-words", config.excludeUncommonWords);
  config.maxSolutions = static_cast<uint32_t>(
      Utils::Input::getArgValue(args, "max-solutions", static_cast<int>(config.maxSolutions)));

  return config;
}

void PoopleGame::printSolutions(const std::vector<Poople::Solution> &solutions,
                                const int limit) {
  if (solutions.empty()) {
    std::cout << "\nNo solutions found.\n";
    return;
  }

  std::cout << "\nFound " << solutions.size() << " solution(s).\n";
  int count = std::min(limit, static_cast<int>(solutions.size()));
  std::cout << "Displaying top " << count << " solutions:\n\n";

  for (int i = 0; i < count; ++i) {
    const auto &sol = solutions[i];
    std::cout << std::setw(3) << (i + 1) << ". (" << sol.words.size()
              << " words, min score: " << std::fixed << std::setprecision(4)
              << sol.minScore << ") ";
    for (size_t w = 0; w < sol.words.size(); ++w) {
      if (w > 0) std::cout << " -> ";
      std::string wordUpper = sol.words[w];
      for (char &c : wordUpper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      std::cout << wordUpper;
    }
    std::cout << "\n";
  }
}

void PoopleGame::runCLI() {
  std::cout << "\n=== Poople (Word Ladder Solver) ===\n";

  while (true) {
    try {
      Poople::Config config = getConfigFromUser();

      std::cout << "\nSolving word ladder from '" << config.startWord
                << "' to '" << config.endWord << "'...\n";

      Poople::SolverResult result = Poople::solve(config);

      if (!result.errorMessage.empty()) {
        std::cerr << "Error: " << result.errorMessage << "\n";
      } else {
        printSolutions(result.solutions, 100);
      }

      while (true) {
        try {
          std::cout << "\nCommands: 's' (solve again), 'a' (show all), 'q' (menu)\n";
          std::cout << "Enter command: ";
          std::string input = Utils::Input::readLine();
          input = Utils::trimToLower(input);

          if (input.empty()) continue;
          if (input == "s" || input == "solve") break;
          if (input == "a" || input == "all") {
            printSolutions(result.solutions, static_cast<int>(result.solutions.size()));
            continue;
          }
          if (input == "q" || input == "quit") return;
        } catch (const Utils::Input::UserCancelledException &) {
          std::cout << "Returning to game menu...\n";
          return;
        }
      }
    } catch (const Utils::Input::UserCancelledException &) {
      std::cout << "Returning to game menu...\n";
      return;
    }
  }
}

void PoopleGame::runHeadless(const Utils::Input::CommandArgs &cmdArgs) {
  try {
    const auto &args = cmdArgs.flags;
    Poople::Config config = getConfigFromArgs(args);

    if (config.startWord.empty()) {
      std::cerr << "Error: --start word is required.\n";
      return;
    }

    if (config.endWord.empty()) {
      if (config.startWord.length() == 4) {
        config.endWord = "poop";
      } else {
        std::cerr << "Error: --end word is required.\n";
        return;
      }
    }

    Poople::SolverResult result = Poople::solve(config);

    if (!result.errorMessage.empty()) {
      std::cerr << "Error: " << result.errorMessage << "\n";
      return;
    }

    std::string outputFile = Utils::Input::getArgValue(args, "o", std::string(""));
    if (outputFile.empty()) {
      outputFile = Utils::Input::getArgValue(args, "output", std::string("results/temp.txt"));
    }

    std::filesystem::path filePath(outputFile);
    if (!filePath.parent_path().empty() && !std::filesystem::exists(filePath.parent_path())) {
      std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream out(outputFile);
    if (out.is_open()) {
      for (const auto &solution : result.solutions) {
        out << solution.text << "\n";
      }
      out.close();
    } else {
      std::cerr << "Could not write to file: " << outputFile << "\n";
    }

    std::cout << result.solutions.size() << "\n";
    std::cout << outputFile << "\n";
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
  }
}

} // namespace Game
