#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
// additional to follow
enum class HashMapEnum { TBB, GROWT };
struct SolverConfig {
  HashMapEnum map;
  int initialHashMapSize;
  int paramB;
  int paramC;
  int paramD;
  int paramE;
};

SolverConfig getSolverConfig(int version) {
  switch (version) {
  case 100:
    return {HashMapEnum::TBB, 100, 20, 30, 40, 50};
  case 101:
    return {HashMapEnum::TBB, 5000000, 20, 30, 40, 50};
  case 200:
    return {HashMapEnum::GROWT, 100, 20, 30, 40, 50};
  case 201:
    return {HashMapEnum::GROWT, 5000000, 20, 30, 40, 50};
  default:
    throw std::invalid_argument("Invalid STVersion: " +
                                std::to_string(version));
  }
}