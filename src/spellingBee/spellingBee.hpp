#pragma once
#include <array>
#include <string>
#include <unordered_set>
#include <vector>

#include "utils/inputUtils.hpp"
#include "utils/utils.hpp"
#include <atomic>

namespace SpellingBee {
struct Config {
  std::vector<char> allLetters;
  std::array<bool, 256> validLettersMap = {false};
  bool excludeUncommonWords = false;
  std::unordered_set<std::string> excludedWords = {};
  bool mustIncludeFirstLetter = true;
  bool reuseLetters = true;
};

struct Result {
  std::vector<Utils::Word> words;
  int totalValidWords = 0;
};

Result runSpellingBeeSolver(const Config &config,
                            std::atomic<bool> *cancel = nullptr);
} // namespace SpellingBee
