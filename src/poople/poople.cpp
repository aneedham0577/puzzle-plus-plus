#include "poople.hpp"

#include <algorithm>
#include <deque>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Poople {

namespace {

// Helper to generate generic bucket patterns for a word (e.g. "poop" -> ["*oop", "p*op", "po*p", "poo*"])
std::vector<std::string> getPatterns(const std::string &word) {
  std::vector<std::string> patterns;
  patterns.reserve(word.size());
  for (size_t i = 0; i < word.size(); ++i) {
    std::string p = word;
    p[i] = '*';
    patterns.push_back(std::move(p));
  }
  return patterns;
}

} // namespace

SolverResult solve(const Config &config, const std::atomic<bool> &cancelToken) {
  SolverResult result;

  std::string start = Utils::trimToLower(config.startWord);
  std::string end = Utils::trimToLower(config.endWord);

  if (start.empty() || end.empty()) {
    result.errorMessage = "Start word and end word cannot be empty.";
    return result;
  }

  if (start.length() != end.length()) {
    result.errorMessage = "Start word and end word must have the same length.";
    return result;
  }

  const size_t wordLen = start.length();

  // If start equals end
  if (start == end) {
    Solution s;
    s.words = {start};
    s.minScore = 1.0;
    for (const auto &w : Utils::g_words) {
      if (w.wordString == start) {
        s.minScore = w.score;
        break;
      }
    }
    s.text = start;
    result.solutions.push_back(s);
    result.totalSolutionsFound = 1;
    return result;
  }

  // Filter dictionary words of the exact same length
  // Map word string to its index in candidate vector
  struct CandidateWord {
    std::string word;
    double score;
  };

  std::vector<CandidateWord> candidates;
  std::unordered_map<std::string, int> wordToIndex;

  // Track if start and end words exist in dictionary
  bool startFound = false;
  bool endFound = false;
  double startScore = 0.0;
  double endScore = 0.0;

  for (const auto &w : Utils::g_words) {
    if (w.wordString.length() != wordLen) {
      continue;
    }

    if (w.wordString == start) {
      startFound = true;
      startScore = w.score;
    }
    if (w.wordString == end) {
      endFound = true;
      endScore = w.score;
    }

    if (config.excludeUncommonWords && !w.is_scrabble) {
      continue;
    }

    int idx = static_cast<int>(candidates.size());
    wordToIndex[w.wordString] = idx;
    candidates.push_back({w.wordString, w.score});
  }

  // Ensure start and end are in the candidate set even if they weren't in common scrabble words
  if (wordToIndex.find(start) == wordToIndex.end()) {
    int idx = static_cast<int>(candidates.size());
    wordToIndex[start] = idx;
    candidates.push_back({start, startScore});
  }
  if (wordToIndex.find(end) == wordToIndex.end()) {
    int idx = static_cast<int>(candidates.size());
    wordToIndex[end] = idx;
    candidates.push_back({end, endScore});
  }

  const int startIdx = wordToIndex[start];
  const int endIdx = wordToIndex[end];
  const size_t numWords = candidates.size();

  // Build pattern buckets
  std::unordered_map<std::string, std::vector<int>> patternMap;
  for (int i = 0; i < static_cast<int>(numWords); ++i) {
    auto patterns = getPatterns(candidates[i].word);
    for (auto &pat : patterns) {
      patternMap[pat].push_back(i);
    }
  }

  // Build adjacency list
  std::vector<std::vector<int>> adj(numWords);
  std::vector<bool> seen(numWords, false);

  for (int i = 0; i < static_cast<int>(numWords); ++i) {
    auto patterns = getPatterns(candidates[i].word);
    seen[i] = true; // don't add self
    for (const auto &pat : patterns) {
      auto it = patternMap.find(pat);
      if (it != patternMap.end()) {
        for (int neighbor : it->second) {
          if (!seen[neighbor]) {
            seen[neighbor] = true;
            adj[i].push_back(neighbor);
          }
        }
      }
    }
    // reset seen for neighbors
    seen[i] = false;
    for (int n : adj[i]) {
      seen[n] = false;
    }
  }

  // Search for paths up to maxDepth words (i.e. length <= maxDepth)
  // Each path starts with 1 word: start. Max depth means path.size() <= config.maxDepth.
  const uint32_t maxDepth = std::max(uint32_t(2), config.maxDepth);

  // First, compute shortest distances from start using BFS to prune paths that cannot reach end
  // Also compute shortest distances from end
  std::vector<int> distFromEnd(numWords, -1);
  {
    std::queue<int> q;
    distFromEnd[endIdx] = 0;
    q.push(endIdx);
    while (!q.empty()) {
      int u = q.front();
      q.pop();
      for (int v : adj[u]) {
        if (distFromEnd[v] == -1) {
          distFromEnd[v] = distFromEnd[u] + 1;
          q.push(v);
        }
      }
    }
  }

  // If end is unreachable at all
  if (distFromEnd[startIdx] == -1 || static_cast<uint32_t>(distFromEnd[startIdx] + 1) > maxDepth) {
    return result;
  }

  // Use Iterative Deepening DFS (IDDFS) bounded by distance-from-target heuristics.
  // This guarantees shortest solutions are found first (length L = minLen, then L = minLen + 1, ...),
  // while consuming O(maxDepth) = O(1) memory instead of O(branching^depth) gigabytes in a queue.
  const uint32_t minLen = static_cast<uint32_t>(distFromEnd[startIdx] + 1);
  const uint32_t maxSearchLen = maxDepth;
  const uint32_t maxSolutions = (config.maxSolutions > 0) ? config.maxSolutions : 100000;

  std::vector<int> currentPath;
  currentPath.reserve(maxSearchLen);
  currentPath.push_back(startIdx);

  std::vector<bool> inCurrentPath(numWords, false);
  inCurrentPath[startIdx] = true;

  auto searchTargetLength = [&](auto &self, int u, uint32_t targetLen) -> void {
    if (cancelToken.load(std::memory_order_relaxed)) {
      result.wasCancelled = true;
      return;
    }

    if (result.solutions.size() >= maxSolutions) {
      return;
    }

    if (currentPath.size() == targetLen) {
      if (u == endIdx) {
        Solution sol;
        sol.words.reserve(currentPath.size());
        double minSc = 1.0;
        std::string text;
        for (size_t i = 0; i < currentPath.size(); ++i) {
          int idx = currentPath[i];
          sol.words.push_back(candidates[idx].word);
          minSc = std::min(minSc, candidates[idx].score);
          if (i > 0) text += " ";
          text += candidates[idx].word;
        }
        sol.minScore = minSc;
        sol.text = std::move(text);
        result.solutions.push_back(std::move(sol));
        result.totalSolutionsFound++;
      }
      return;
    }

    int remainingSteps = static_cast<int>(targetLen - currentPath.size());
    for (int next : adj[u]) {
      if (inCurrentPath[next]) {
        continue;
      }
      // Heuristic prune: can `next` reach `end` in remainingSteps - 1?
      if (distFromEnd[next] > remainingSteps - 1) {
        continue;
      }

      inCurrentPath[next] = true;
      currentPath.push_back(next);
      self(self, next, targetLen);
      currentPath.pop_back();
      inCurrentPath[next] = false;

      if (result.wasCancelled || result.solutions.size() >= maxSolutions) {
        return;
      }
    }
  };

  for (uint32_t len = minLen; len <= maxSearchLen; ++len) {
    searchTargetLength(searchTargetLength, startIdx, len);
    if (result.wasCancelled || result.solutions.size() >= maxSolutions) {
      break;
    }
  }

  // Sort solutions according to spec:
  // 1. Length ascending
  // 2. minScore descending
  // 3. words lexicographical
  std::sort(result.solutions.begin(), result.solutions.end());

  return result;
}

} // namespace Poople
