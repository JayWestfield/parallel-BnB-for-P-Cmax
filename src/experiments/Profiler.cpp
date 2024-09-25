
#include <iostream>
#include "./../BnB/BnB_base.cpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h>
#include <string>
#include <gperftools/profiler.h>
#include "./readData/readData.h"
#include <valgrind/callgrind.h>

int main(int argc, char *argv[])
{
    setenv("CPUPROFILE_FREQUENCY", "1000", 1); // Set the sampling frequency to 1000 Hz (that somehow does not work)

    CALLGRIND_TOGGLE_COLLECT; // Stop collecting data
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
    BnB_base_Impl solver(true, true, true, false, STVersion);

    std::condition_variable cv;
    std::mutex mtx;
    int numJobs, numMachines;
    std::vector<int> jobDurations;
    std::string instanceFilePath = basePath + "/" + benchmark + "/" + instanceName;
    readData.readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
    std::sort(jobDurations.begin(), jobDurations.end(), std::greater<int>());
    assert(std::is_sorted(jobDurations.begin(), jobDurations.end(), std::greater<int>()));
    std::cout << instanceName << std::endl;
    solver.cleanUp();

    tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, ThreadsToUse);
    int result = 0;
    CALLGRIND_TOGGLE_COLLECT;                      // Start collecting data
    for (int i = 0; i < repeatInstance; i++)
    {
        std::string profile_file = "./profiling_results/profile_" + std::to_string(i) + ".prof";
        ProfilerStart(profile_file.c_str());
        auto start = std::chrono::high_resolution_clock::now();
        result = solver.solve(numMachines, jobDurations);
        auto end = std::chrono::high_resolution_clock::now();
        ProfilerStop();

        CALLGRIND_TOGGLE_COLLECT; // Stop collecting data
        if (result != optimalSolutions.find(instanceName)->second)
        {
            std::cout << " error_wrong_makespan_of_" << result << " round " << i << " with nnodes " << solver.visitedNodes << std::endl;
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
