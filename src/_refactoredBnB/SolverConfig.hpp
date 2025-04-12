#include <cmath>
#include <stdexcept>
#include <string>
// additional to follow
struct SolverConfig {
  int initialHashMapSize;
  int GistStorageStackSize;
  int notInsertingGists;
  int templateOptimization;
  int paramE;
};

SolverConfig getSolverConfig(int version) {
  // decode version
  int config = version % 100;
  int templateOptimization =
      ((version / 1000) % 10) * 10 + (version / 100) % 10;
  switch (config) {
  case 0:
    return {static_cast<int>(pow(2, 7)), 100, 1, templateOptimization, 0};
  case 1:
    return {static_cast<int>(pow(2, 10)), 100, 1, templateOptimization, 0};
  case 2:
    return {static_cast<int>(pow(2, 13)), 100, 1, templateOptimization, 0};
  case 3:
    return {static_cast<int>(pow(2, 16)), 100, 1, templateOptimization, 0};
  case 4:
    return {static_cast<int>(pow(2, 19)), 100, 1, templateOptimization, 0};
  case 5:
    return {static_cast<int>(pow(2, 21)), 100, 1, templateOptimization, 0};
  case 6:
    return {static_cast<int>(pow(2, 24)), 100, 1, templateOptimization, 0};
  default:
    throw std::invalid_argument("Invalid STVersion: " +
                                std::to_string(version));
  }
}