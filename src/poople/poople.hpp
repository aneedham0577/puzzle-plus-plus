#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#include "utils/utils.hpp"

namespace Poople {

// Configuration for the Poople word ladder solver
struct Config {
  std::string startWord;
  std::string endWord;
  uint32_t maxDepth = 10;
  bool excludeUncommonWords = true;
  uint32_t maxSolutions = 1000;
  int preset = 1; // 0=Custom, 1=Default (depth 10, common words), 2=Fast (first 100), 3=Exhaustive (all words)
};

// Represents a valid word ladder solution path
struct Solution {
  std::vector<std::string> words;
  double minScore = 0.0;
  std::string text; // Space-separated word sequence

  bool operator<(const Solution &other) const {
    if (words.size() != other.words.size()) {
      return words.size() < other.words.size(); // Ascending length
    }
    if (minScore != other.minScore) {
      return minScore > other.minScore; // Descending min word score
    }
    return words < other.words;
  }
};

// Solver result container
struct SolverResult {
  std::vector<Solution> solutions;
  size_t totalSolutionsFound = 0;
  bool wasCancelled = false;
  std::string errorMessage;
};

// Main solver function
SolverResult solve(const Config &config,
                   const std::atomic<bool> &cancelToken = false);

} // namespace Poople
