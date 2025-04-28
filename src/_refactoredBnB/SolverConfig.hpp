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
  case 7:
    return {static_cast<int>(pow(2, 27)), 100, 1, templateOptimization, 0};
  case 8:
    return {static_cast<int>(pow(2, 30)), 100, 1, templateOptimization, 0};
  case 9:
    return {static_cast<int>(pow(2, 33)), 100, 1, templateOptimization, 0};
  case 10:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 4)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 11:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 6)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 12:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 8)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 13:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 14:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 15:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 12)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 16:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 14)), 1,
            templateOptimization, 0};
  case 17:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 0)), templateOptimization, 0};
  case 18:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 1)), templateOptimization, 0};
  case 19:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 2)), templateOptimization, 0};
  case 20:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 3)), templateOptimization, 0};
  case 21:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 4)), templateOptimization, 0};
  case 22:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)),
            static_cast<int>(pow(2, 5)), templateOptimization, 0};
  case 23:
    return {static_cast<int>(pow(2, 16)), static_cast<int>(pow(2, 10)), 0,
            templateOptimization, 0};
  default:
    throw std::invalid_argument("Invalid STVersion: " +
                                std::to_string(version));
  }
}