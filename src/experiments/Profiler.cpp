
#include <iostream>
#include "./../BnB/BnB_base.cpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h>
#include <string>
#include <gperftools/profiler.h>
#include "./readData/readData.h"

int main(int argc, char *argv[])
{
    Parser readData;
    int ThreadsToUse = std::stoi(argv[1]);
    int STVersion = 0;
    if (argc >= 3)
        STVersion = std::stoi(argv[2]);
    std::string basePath = "benchmarks";
    std::string instanceName = "p_cmax-class1-n90-m40-minsize1-maxsize100-seed21578.txt";
    if (argc >= 4)
        instanceName = argv[3];

    std::string benchmark = "lawrinenko";
    if (argc >= 5)
        benchmark = argv[4];

    std::unordered_map<std::string, int> optimalSolutions;
    readData.readOptimalSolutions(basePath + "/opt-known-instances-" + benchmark + ".txt", optimalSolutions);
    std::vector<std::string> instances_to_solve = {instanceName};
    BnB_base_Impl solver(true, true, true, false, STVersion);

    std::condition_variable cv;
    std::mutex mtx;
    int numJobs, numMachines;
    std::vector<int> jobDurations;
    std::string instanceFilePath = basePath + "/" + benchmark + "/" + instanceName;
    readData.readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
    std::cout << instanceName << std::endl;

    tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, ThreadsToUse);
    int result = 0;
    auto start = std::chrono::high_resolution_clock::now();
    result = solver.solve(numMachines, jobDurations);
    auto end = std::chrono::high_resolution_clock::now();

    if (result != optimalSolutions.find(instanceName)->second)
        std::cout << " error_wrong_makespan_of_" << result;
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


    std::cout << std::endl;

    return 0;
}
