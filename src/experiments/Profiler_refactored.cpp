// #include "../_refactoredBnB/ST/Hashmaps/GrowtHashMap_refactored.hpp"
#include "../_refactoredBnB/ST/Hashmaps/TBBHashMap_refactored.hpp"
#include "../_refactoredBnB/SolverConfig.hpp"
#include "_refactoredBnB/ST/Hashmaps/GrowtHashMap_refactored.hpp"
#include "_refactoredBnB/Solver/solver_base.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include "experiments/readData/readData.h"
#include <chrono>
#include <future>
#include <gperftools/profiler.h>
#include <string>
struct instanceConfig {
  SolverConfig solverConfig;
  int timeout;
  int numThreads;
  std::string instancePath;
  int optimalSolution;
};
instanceConfig parseArguments(int argc, char *argv[]) {
  if (argc != 6) {
    std::cout << "argc: " << argc << std::endl;
    throw std::invalid_argument(
        "Usage: ./solver <STVersion> <repetitions> <numThreads> <instancePath> "
        "<optimalSolution>");
  }

  instanceConfig config;

  try {
    config.solverConfig = getSolverConfig(std::stoi(argv[1]));
    config.timeout = std::stoi(argv[2]);
    config.numThreads = std::stoi(argv[3]);
    config.instancePath = argv[4];
    config.optimalSolution = std::stoi(argv[5]);
  } catch (const std::exception &e) {
    throw std::invalid_argument("Error parsing arguments: " +
                                std::string(e.what()));
  }

  return config;
}

Instance getInstance(std::string instancePath) {
  Parser readData;
  Instance instance;
  readData.readInstance(instancePath, instance.numJobs, instance.numMachines,
                        instance.jobDurations);

  std::sort(instance.jobDurations.begin(), instance.jobDurations.end(),
            std::greater<int>());
  return instance;
}
int main(int argc, char *argv[]) {
  instanceConfig config = parseArguments(argc, argv);
  std::future<void> canceler;
  std::condition_variable cv;
  std::mutex mtx;
  int result = 0;
  Instance instance = getInstance(config.instancePath);
  constexpr Logging noLogs{false, false, false, false};
  constexpr Logging allLogs{true, true, true, true};
  // note the addPrev optimization has problems somehow????ß
  constexpr Optimizations allOpts{true, true, true, true, true};
  constexpr Config myConfig{allOpts, noLogs};
  // TODO big switch for the correct solver

  for (int i = 0; i < config.timeout; i++) {
    // solver_base<TBBHashMap_refactored<myConfig.optimizations.use_fingerprint>,
    //             myConfig>
    //     solver(config.numThreads);
    solver_base<GrowtHashMap_refactored<myConfig.optimizations.use_fingerprint>,
                myConfig>
        solver(config.numThreads);
    std::string profile_file =
        "./profiling_results/profile_" + std::to_string(i) + ".prof";
    ProfilerStart(profile_file.c_str());
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();
    ProfilerStop();

    if (result != config.optimalSolution) {
      std::cout << " error_wrong_makespan_of_ " << result << " instead of "
                << config.optimalSolution << " round " << i << " with nodes "
                << solver.visitedNodes << std::endl;
      return 1;
    } else {
      std::string times = "{";
      for (std::size_t i = std::max<int>(solver.timeFrames.size() - 5, 0);
           i < solver.timeFrames.size(); ++i) {
        times += std::to_string(solver.timeFrames[i].count()) + ";";
      }
      times.pop_back();
      times.append("}");
      std::cout << " ("
                << ((std::chrono::duration<double>)(end - start)).count() << ","
                << solver.visitedNodes << "," << times << ","
                << (int)solver.hardness << ")";
    }

    std::cout << std::endl;
  }
}