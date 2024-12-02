
#include <iostream>
#include "./../customBnB/customBnB_base.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h>
#include <string>
#include "./readData/readData.h"
#include <valgrind/callgrind.h>

int main(int argc, char *argv[])
{

    Parser readData;
    int ThreadsToUse = std::stoi(argv[1]);
    int repeatInstance = 1;
    if (argc >= 3)
        repeatInstance = std::stoi(argv[2]);
    int STVersion = 0;
    if (argc >= 4)
        STVersion = std::stoi(argv[3]);
    std::string basePath = "benchmarks";
    std::string instanceName = "p_cmax-class1-n90-m40-minsize1-maxsize100-seed21578.txt";
    if (argc >= 5)
        instanceName = argv[4];

    std::string benchmark = "lawrinenko";
    if (argc >= 6)
        benchmark = argv[5];

    std::unordered_map<std::string, int> optimalSolutions;
    readData.readOptimalSolutions(basePath + "/opt-known-instances-" + benchmark + ".txt", optimalSolutions);
    std::vector<std::string> instances_to_solve = {instanceName};
    BnB_base_custom_work_stealing_iterative solver(true, true, true, true, STVersion, ThreadsToUse);

    std::condition_variable cv;
    std::mutex mtx;
    int numJobs, numMachines;
    std::vector<int> jobDurations;
    std::string instanceFilePath = basePath + "/" + benchmark + "/" + instanceName;
    readData.readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
    std::sort(jobDurations.begin(), jobDurations.end(), std::greater<int>());
    assert(std::is_sorted(jobDurations.begin(), jobDurations.end(), std::greater<int>()));
    std::cout << instanceName << std::flush;
    solver.cleanUp();

    tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, ThreadsToUse);
    int result = 0;
    for (int i = 0; i < repeatInstance; i++)
    {
        auto start = std::chrono::high_resolution_clock::now();
        result = solver.solve(numMachines, jobDurations);
        auto end = std::chrono::high_resolution_clock::now();

        if (result != optimalSolutions.find(instanceName)->second)
        {
            std::cout << " error_wrong_makespan_of_ " << result << " instead of " << optimalSolutions.find(instanceName)->second <<  " round " << i << " with nnodes " << solver.visitedNodes << std::endl;
            return 1;
        }
        else
        {
            std::string times = "{";
            for (auto time : solver.timeFrames)
            {
                times += std::to_string(time.count()) + ";";
            }
            times.pop_back();
            times.append("}");
            std::cout << " (" << ((std::chrono::duration<double>)(end - start)).count() << "," << solver.visitedNodes << "," << times << "," << (int)solver.hardness << ")";
        }
        solver.cleanUp();

        std::cout << std::endl;
    }
    // std::this_thread::sleep_for(std::chrono::seconds(5));

    return 0;
}
