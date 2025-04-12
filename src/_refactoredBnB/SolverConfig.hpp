#include <fstream>
#include <iostream>
#include <sstream>
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
    return {100, 100, 1, templateOptimization, 0};
  case 1:
    return {500000, 100, 1, templateOptimization, 0};
  case 2:
    return {50000000, 100, 1, templateOptimization, 0};
  default:
    throw std::invalid_argument("Invalid STVersion: " +
                                std::to_string(version));
  }
}