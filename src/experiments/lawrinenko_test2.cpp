#include <algorithm>
#include <cstddef>
#include <iostream>
// #include "./../BnB/BnB_base.cpp"

#include "../customBnB/customBnB_base.hpp"
#include "./readData/readData.h"
#include <chrono>
#include <fstream>
#include <future>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <tbb/tbb.h>
int main(int argc, char *argv[]) {
  Parser readData;
  int maxThreads = std::stoi(argv[1]);
  int timeout = 1;
  if (argc >= 3)
    timeout = std::stoi(argv[2]);
  int STVersion = 3;
  if (argc >= 4)
    STVersion = std::stoi(argv[3]);
  std::string basePath = "benchmarks";
  if (argc >= 5)
    basePath = argv[4];

  std::string benchmark = "lawrinenko";
  if (argc >= 6)
    benchmark = argv[5];
  std::string path_to_selection_of_instances =
      "src/experiments/filtered_instances_new_without1.txt";
  if (argc >= 7)
    path_to_selection_of_instances = argv[6];
  std::unordered_map<std::string, int> optimalSolutions;
  readData.readOptimalSolutions(basePath + "/opt-known-instances-" + benchmark +
                                    ".txt",
                                optimalSolutions);
  std::vector<std::string> instances_to_solve = {};
  if (path_to_selection_of_instances != "all") {
    std::ifstream infile(path_to_selection_of_instances);
    if (!infile) {
      throw std::runtime_error("Unable to open file");
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

  bool ownProcess = true;
  for (auto instanceName : instances_to_solve) {
    int numJobs, numMachines;
    std::vector<int> jobDurations;

    std::string instanceFilePath =
        basePath + "/" + benchmark + "/" + instanceName;
    readData.readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
    std::sort(jobDurations.begin(), jobDurations.end(), std::greater<int>());
    int numThreads = 1;
    std::condition_variable cv;
    std::mutex mtx;
    std::cout << instanceName << std::flush;
    while (numThreads <= maxThreads) {
      pid_t pid = 0;
      if (ownProcess) {
        pid = fork();
        if (pid < 0) {
          std::cerr << "Fork failed" << std::endl;
          return 1;
        }
      }

      if (pid == 0) {
        tbb::global_control global_limit(
            tbb::global_control::max_allowed_parallelism, numThreads);
        std::future<void> canceler;
        int result = 0;
        // BnB_base_Impl solver(true, true, true, true, 3, numThreads);
        BnB_base_custom_work_stealing_iterative solver(true, true, true, true,
                                                       STVersion, numThreads);
        bool timerExpired = false;
        canceler = std::async(std::launch::async, [&solver, &result, &cv, &mtx,
                                                   &timerExpired, &timeout]() {
          std::unique_lock<std::mutex> lock(mtx);
          if (cv.wait_for(lock, std::chrono::seconds(timeout),
                          [&timerExpired] { return timerExpired; })) {
            return;
          }
          if (result == 0) {
            solver.cancelExecution();
          }
        });
        solver.cleanUp();
        auto start = std::chrono::high_resolution_clock::now();
        result = solver.solve(numMachines, jobDurations);
        auto end = std::chrono::high_resolution_clock::now();

        {
          std::lock_guard<std::mutex> lock(mtx);
          timerExpired = true;
        }
        cv.notify_all();
        canceler.get();
        if (result == 0)
          std::cout << " (canceled)";
        else if (result != optimalSolutions.find(instanceName)->second)
          std::cout << " error_wrong_makespan_of_" << result << "_instead_of"
                    << optimalSolutions.find(instanceName)->second
                    << " caceled?" << solver.cancel;
        else {
          std::string times = "{";
          // only print the last 5 bounds (makes it easier to read)
          for (std::size_t i = std::max<int>(solver.timeFrames.size() - 5, 0);
               i < solver.timeFrames.size(); ++i) {
            times += std::to_string(solver.timeFrames[i].count()) + ";";
          }
          // for (auto time : solver.timeFrames) {
          //   times += std::to_string(time.count()) + ";";
          // }
          times.pop_back();
          times.append("}");
          std::cout << " ("
                    << ((std::chrono::duration<double>)(end - start)).count()
                    << "," << solver.visitedNodes << "," << times << ","
                    << (int)solver.hardness << ")" << std::flush;
        }
        std::cout << std::flush;
        solver.cleanUp();
        if (ownProcess)
          return 0;
      } else {
        int status;
        waitpid(pid, &status, 0);
      }
      // ab 8 threads plus 8 für bessere auflösung
      numThreads += (numThreads > 8) ? 8 : numThreads;
    }
    std::cout << std::endl;
  }
  return 0;
}
