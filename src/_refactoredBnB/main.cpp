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
  auto solver =
      config.SolverConfig / 10000 == 1 ? "executableTBB " : "executableGrowt ";
  std::cout << "Start Experiment with: [Config] maxThreads="
            << config.maxThreads << ", timeout=" << config.timeout
            << ", STVersion=" << solver << config.SolverConfig << ", basePath='"
            << config.basePath << "'" << ", benchmark='" << config.benchmark
            << "'" << ", instanceSelectionPath='"
            << config.instanceSelectionPath << "'" << std::endl;
}
int main(int argc, char *argv[]) {
  Parser readData;
  std::string executableTBB = "build/RefactoredBnBTBB";
  std::string executableGrowt = "build/RefactoredBnBGrowt";

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
    // Vector to store futures for concurrent execution
    std::vector<std::future<std::pair<int, std::string>>> futures;

    // Launch commands for 1, 2, 4, and 8 threads concurrently
    for (int i = 1; i <= std::min(8, config.maxThreads); i *= 2) {
      std::ostringstream argsStream;
      argsStream << config.SolverConfig << " " << config.timeout << " " << i
                 << " " << instancePath << " "
                 << optimalSolutions.at(instanceName);
      auto executable =
          config.SolverConfig / 10000 == 1 ? executableTBB : executableGrowt;
      std::string command = executable + " " + argsStream.str();

      // Launch the command asynchronously
      futures.push_back(std::async(std::launch::async, [command]() {
        std::array<char, 128> buffer;
        std::string result;
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) {
          throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
          result += buffer.data();
        }
        int returnCode = pclose(pipe);
        return std::make_pair(returnCode, result);
      }));
    }

    // Wait for all commands to finish and print their outputs in order
    for (size_t i = 0; i < futures.size(); ++i) {
      auto [returnCode, output] = futures[i].get();
      if (returnCode != 0) {
        std::cerr << "Command for " << (1 << i)
                  << " threads failed with code: " << returnCode << std::endl;
      }
      std::cout << output << std::flush;
    }
    for (int i = 16; i <= config.maxThreads; i *= 2) {
      std::ostringstream argsStream;
      argsStream << config.SolverConfig << " " << config.timeout << " " << i
                 << " " << instancePath << " "
                 << optimalSolutions.at(instanceName);
      auto executable =
          config.SolverConfig / 10000 == 1 ? executableTBB : executableGrowt;
      std::string command = executable + " " + argsStream.str();

      int returnCode = std::system(command.c_str());
      if (returnCode != 0) {
        std::cerr << "Command failed with code: " << returnCode << std::endl;
      }
    }
    std::cout << std::endl;
  }
}