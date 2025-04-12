#include "./SolverConfig.hpp"
#include "_refactoredBnB/ST/Hashmaps/GrowtHashMap_refactored.hpp"
#include "_refactoredBnB/ST/Hashmaps/TBBHashMap_refactored.hpp"
#include "_refactoredBnB/Solver/solver_base.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include "experiments/readData/readData.h"
#include <chrono>
#include <future>
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
    throw std::invalid_argument(
        "Usage: ./solver <STVersion> <timeout> <numThreads> <instancePath> "
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
void produceOutput(int result, instanceConfig config, double duration,
                   std::vector<std::chrono::duration<double>> &timeFrames,
                   uint64_t visitedNodes, Difficulty hardness) {
  if (result == 0)
    std::cout << " (canceled)";
  else if (result != config.optimalSolution)
    std::cout << " error_wrong_makespan_of_" << result << "_instead_of"
              << config.optimalSolution;
  else {
    std::string times = "{";
    for (std::size_t i = std::max<int>(timeFrames.size() - 5, 0);
         i < timeFrames.size(); ++i) {
      times += std::to_string(timeFrames[i].count()) + ";";
    }

    if (hardness != Difficulty::trivial)
      times.pop_back();
    times.append("}");
    std::cout << " (" << duration << "," << visitedNodes << "," << times << ","
              << (int)hardness << ")" << std::flush;
  }
  std::cout << std::flush;
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
  constexpr Logging myLogs = noLogs;
  auto solverConfig = config.solverConfig;
  switch (solverConfig.templateOptimization) {
  case 0: {
    constexpr Optimizations myOpts{true, true, true, true, false, true, true};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 1: {
    constexpr Optimizations myOpts{true,  true,  true, false,
                                   false, false, false};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 2: {
    constexpr Optimizations myOpts{true, true, true, true, false, false, false};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 3: {
    constexpr Optimizations myOpts{true, true, true, true, false, true, false};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 4: {
    constexpr Optimizations myOpts{true, true, true, true, false, true, true};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 5: {
    constexpr Optimizations myOpts{true, true, true, true, true, true, true};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 6: {
    constexpr Optimizations myOpts{true, true, true, true, false, true, true};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  case 7: {
    constexpr Optimizations myOpts{true, true, true, true, false, true, true};
    constexpr Config myConfig{myOpts, myLogs};
    solver_base<
        GrowtHashMap_refactored<myOpts.use_fingerprint, myOpts.use_max_offset>,
        myConfig>
        solver(config.numThreads, solverConfig.initialHashMapSize,
               solverConfig.notInsertingGists,
               solverConfig.GistStorageStackSize);

    bool timerExpired = false;
    canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                               &timerExpired, &config]() {
      std::unique_lock<std::mutex> lock(mtx);
      if (cv.wait_for(lock, std::chrono::seconds(config.timeout),
                      [&timerExpired] { return timerExpired; })) {
        return;
      }
      if (result == 0) {
        solver.cancelExecution();
      }
    });
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(instance);
    auto end = std::chrono::high_resolution_clock::now();

    {
      std::lock_guard<std::mutex> lock(mtx);
      timerExpired = true;
    }
    cv.notify_all();
    canceler.get();
    assert(result == config.optimalSolution);

    produceOutput(
        result, config,
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
            .count(),
        solver.timeFrames, solver.visitedNodes, solver.hardness);
  } break;
  }
}
