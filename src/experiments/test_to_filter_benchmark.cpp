#include "../customBnB/customBnB_base.hpp"
#include "./readData/readData.h"
#include <iostream>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <fstream>
#include <future>
#include <iostream>
#include <ostream>
#include <signal.h>
#include <sstream>
#include <string>
#include <tbb/tbb.h>
// #include "./../BnB/BnB_base.cpp"
#include <sys/wait.h>

void readInstance(const std::string &filename, int &numJobs, int &numMachines,
                  std::vector<int> &jobDurations) {
  std::ifstream infile(filename);
  if (!infile) {
    throw std::runtime_error("Unable to open file");
  }
  std::string line;

  // Read the first line
  if (std::getline(infile, line)) {
    std::istringstream iss(line);
    std::string problemType;
    iss >> problemType >> problemType >> numJobs >> numMachines;
  }

  // Read the second line
  if (std::getline(infile, line)) {
    std::istringstream iss(line);
    int duration;
    while (iss >> duration) {
      jobDurations.push_back(duration);
    }
  }
}
void readOptimalSolutions(
    const std::string &filename,
    std::unordered_map<std::string, int> &optimalSolutions) {
  std::ifstream infile(filename);
  if (!infile) {
    throw std::runtime_error("Unable to open file");
  }

  std::string line;
  while (std::getline(infile, line)) {
    std::istringstream iss(line);
    std::string name;
    int optimalMakespan;
    if (iss >> name >> optimalMakespan) {
      optimalSolutions[name] = optimalMakespan;
    }
  }
}

int main(int argc, char *argv[]) {
  int maxThreads = std::stoi(argv[1]);
  int timeout = 1;
  if (argc >= 3)
    timeout = std::stoi(argv[2]);
  std::string benchmark = "lawrinenko";
  if (argc >= 4)
    benchmark = argv[3];

  std::string contains = "";
  if (argc >= 5)
    contains = argv[4];
  int instances = 0;
  std::unordered_map<std::string, int> optimalSolutions;
  readOptimalSolutions("benchmarks/opt-known-instances-" + benchmark + ".txt",
                       optimalSolutions);
  std::vector<std::string> instances_to_solve = {};
  std::cout << "found " << optimalSolutions.size() << " elements" << std::endl;
  for (auto &instance : optimalSolutions) {
    // std::this_thread::sleep_for(std::chrono::seconds(2));
    pid_t pid = 0;
    if (true) {
      pid = fork();
      if (pid < 0) {
        std::cerr << "Fork failed" << std::endl;
        return 1;
      }
    }

    if (pid == 0) {
      auto instanceName = instance.first;
      int numJobs, numMachines;
      std::vector<int> jobDurations;

      std::string instanceFilePath =
          "benchmarks/" + benchmark + "/" + instanceName;
      readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
      std::sort(jobDurations.begin(), jobDurations.end());
      std::condition_variable cv;
      std::mutex mtx;
      // if (instanceName.find("n88-") != std::string::npos ||
      // instanceName.find("n80-") != std::string::npos ||
      // instanceName.find("n90-") != std::string::npos) continue; if
      // (instanceName.find(contains) == std::string::npos) continue;
      // if(instanceName !=
      // "p_cmax-class7-n220-m80-mu880-sigma220-seed30445.txt") continue;

      std::cout << instanceName << std::flush;
      BnB_base_custom_work_stealing_iterative solver(true, true, true, true, 5,
                                                     maxThreads);
      std::future<void> canceler;
      int result = 0;

      bool timerExpired = false;
      canceler = std::async(std::launch::async, [&]() {
        std::unique_lock<std::mutex> lock(mtx);
        if (cv.wait_for(lock, std::chrono::seconds(timeout),
                        [&timerExpired] { return timerExpired; })) {
          return;
        }
        if (result == 0) {
          solver.cancelExecution();
        }
      });
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
        std::cout << " error_wrong_makespan_of_" << result;
      else
        std::cout << " ("
                  << ((std::chrono::duration<double>)(end - start)).count()
                  << "," << solver.visitedNodes << "," << (int)solver.hardness
                  << ")";
      std::cout << std::endl;
      instances++;
      return 0;
    } else {
      int status;
      waitpid(pid, &status, 0);
    }
  }
  std::cout << "Computed a Total of  " << instances << " instances"
            << std::endl;

  return 0;
}
