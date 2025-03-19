#pragma once
#include <vector>

/**
 * @brief describe what optimizations to use (might be extended)
 */
struct Optimizations {
  bool use_irrelevance = true;
  bool use_fur = true;
  bool use_gists = true;
  bool use_add_previously = true;
  bool use_fingerprint = false;
  bool use_max_offset = true;
};
struct Logging {
  // Logging
  bool logNodes = false;
  bool logInitialBounds = false;
  bool logBound = false;
  bool detailedLogging = false;
};
struct Config {
  constexpr Config(Optimizations o, Logging l) : optimizations(o), logging(l) {}
  const Optimizations optimizations;
  const Logging logging;
};

struct Instance {
  int numJobs;
  int numMachines;
  std::vector<int> jobDurations;
};

enum class FindGistResult { COMPLETED, STARTED, NOT_FOUND };
