#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
// additional to follow
struct SolverConfig {
  int initialHashMapSize;
  int GistStorageStackSize;
  int notInsertingGists;
  int paramD;
  int paramE;
};

SolverConfig getSolverConfig(int version) {
  int config = version % 100;
  switch (config) {
  case 0:
    return {100, 100, 1, 0, 0};
  case 1:
    return {500000, 100, 1, 0, 0};
  case 2:
    return {50000000, 100, 1, 0, 0};
  default:
    throw std::invalid_argument("Invalid STVersion: " +
                                std::to_string(version));
  }
}