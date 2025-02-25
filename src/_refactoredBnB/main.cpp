#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <functional>
#include <iostream>

#include "../experiments/readData/readData.h"
#include "./SolverConfig.hpp"
#include <chrono>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <tbb/tbb.h>
#include <vector>
struct Config {
  int maxThreads = 1;
  int timeout = 1;
  int SolverConfig = 100;
  std::string basePath = "benchmarks";
  std::string benchmark = "lawrinenko";
  std::string instanceSelectionPath =
      "src/experiments/filtered_instances_new_without1.txt";
};

Config parseArguments(int argc, char *argv[]) {
  Config config;

  try {
    if (argc >= 2)
      config.maxThreads = std::stoi(argv[1]);
    if (argc >= 3)
      config.timeout = std::stoi(argv[2]);
    if (argc >= 4)
      config.SolverConfig = std::stoi(argv[3]);
    if (argc >= 5)
      config.basePath = argv[4];
    if (argc >= 6)
      config.benchmark = argv[5];
    if (argc >= 7)
      config.instanceSelectionPath = argv[6];
  } catch (const std::invalid_argument &e) {
    std::cerr << "Invalid argument: " << e.what() << "\n";
    std::exit(EXIT_FAILURE);
  } catch (const std::out_of_range &e) {
    std::cerr << "Argument out of range: " << e.what() << "\n";
    std::exit(EXIT_FAILURE);
  }

  return config;
}
std::vector<std::string> getInstancesToSolve(Config config,
                                             auto optimalSolutions) {
  std::vector<std::string> instances_to_solve = {};
  if (config.instanceSelectionPath != "all") {
    std::ifstream infile(config.instanceSelectionPath);
    if (!infile) {
      throw std::runtime_error("Unable to open file" +
                               config.instanceSelectionPath);
    }
    std::string line;
    while (std::getline(infile, line)) {
      std::istringstream iss(line);
      std::string name;
      if (iss >> name) {
        instances_to_solve.push_back(name);
      }
    }
  } else {
    for (const auto &optimal : optimalSolutions)
      instances_to_solve.push_back(optimal.first);
  }
  return instances_to_solve;
}

void printConfigInline(const Config &config) {
  std::cout << "Start Experiment with: [Config] maxThreads="
            << config.maxThreads << ", timeout=" << config.timeout
            << ", STVersion=" << config.SolverConfig << ", basePath='"
            << config.basePath << "'" << ", benchmark='" << config.benchmark
            << "'" << ", instanceSelectionPath='"
            << config.instanceSelectionPath << "'" << std::endl;
}
int main(int argc, char *argv[]) {
  Parser readData;
  std::string executable = "build/RefactoredBnB";
  Config config = parseArguments(argc, argv);
  printConfigInline(config);

  std::unordered_map<std::string, int> optimalSolutions;
  readData.readOptimalSolutions(config.basePath + "/opt-known-instances-" +
                                    config.benchmark + ".txt",
                                optimalSolutions);
  std::vector<std::string> instances_to_solve =
      getInstancesToSolve(config, optimalSolutions);
  bool ownProcess = true;
  for (auto instanceName : instances_to_solve) {
    std::cout << instanceName << std::flush;
    std::string instancePath =
        config.basePath + "/" + config.benchmark + "/" + instanceName + " ";
    // TODO might adapt growth factor
    for (int i = 1; i <= config.maxThreads; i *= 2) {
      std::ostringstream argsStream;
      argsStream << config.SolverConfig << " " << config.timeout << " " << i
                 << " " << instancePath << " "
                 << optimalSolutions.at(instanceName);

      std::string command = executable + " " + argsStream.str();

      int returnCode = std::system(command.c_str());
      if (returnCode != 0) {
        std::cerr << "Command failed with code: " << returnCode << std::endl;
      }
    }
    std::cout << std::endl;
  }
}