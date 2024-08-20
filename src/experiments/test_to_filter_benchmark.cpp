#include <iostream>
#include <iostream>
#include "./../BnB/BnB_base.cpp"
#include <signal.h>
#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h>
#include <future>
#include "./../BnB/BnB.h"
#include <string>

void readInstance(const std::string &filename, int &numJobs, int &numMachines, std::vector<int> &jobDurations)
{
    std::ifstream infile(filename);
    if (!infile)
    {
        throw std::runtime_error("Unable to open file");
    }
    std::string line;

    // Read the first line
    if (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string problemType;
        iss >> problemType >> problemType >> numJobs >> numMachines;
    }

    // Read the second line
    if (std::getline(infile, line))
    {
        std::istringstream iss(line);
        int duration;
        while (iss >> duration)
        {
            jobDurations.push_back(duration);
        }
    }
}
void readOptimalSolutions(const std::string &filename, std::unordered_map<std::string, int> &optimalSolutions)
{
    std::ifstream infile(filename);
    if (!infile)
    {
        throw std::runtime_error("Unable to open file");
    }

    std::string line;
    while (std::getline(infile, line))
    {
        std::istringstream iss(line);
        std::string name;
        int optimalMakespan;
        if (iss >> name >> optimalMakespan)
        {
            optimalSolutions[name] = optimalMakespan;
        }
    }
}


int main(int argc, char *argv[])
{   
    int maxThreads = std::stoi(argv[1]);
    int timeout = 1;
    if (argc >= 3)
        timeout = std::stoi(argv[2]);
    std::string benchmark = "lawrinenko";
    if (argc >= 4)
        benchmark = argv[3];

    std::unordered_map<std::string, int> optimalSolutions;
    readOptimalSolutions("benchmarks/opt-known-instances-" + benchmark + ".txt", optimalSolutions);
    std::vector<std::string> instances_to_solve = {};
    BnB_base_Impl solver(true, true, true, false, false);

    for (auto &instance : optimalSolutions)
    {
        // std::this_thread::sleep_for(std::chrono::seconds(2));

        auto instanceName = instance.first;
        int numJobs, numMachines;
        std::vector<int> jobDurations;

        std::string instanceFilePath = "benchmarks/" + benchmark + "/" + instanceName;
        readInstance(instanceFilePath, numJobs, numMachines, jobDurations);
        std::condition_variable cv;
        std::mutex mtx;
        // if (instanceName.find("n88-") != std::string::npos || instanceName.find("n80-") != std::string::npos || instanceName.find("n90-") != std::string::npos) continue;
        // if (instanceName.find("class7") == std::string::npos) continue;
        // if(instanceName != "p_cmax-class7-n80-m32-mu320-sigma80-seed24362.txt") continue;

        std::cout << instanceName;

        tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, maxThreads);
        std::future<void> canceler;
        int result = 0;

        bool timerExpired = false;
        canceler = std::async(std::launch::async, [&]()
                              {
                    std::unique_lock<std::mutex> lock(mtx);
                    if(cv.wait_for(lock, std::chrono::seconds(timeout), [&timerExpired]{ return timerExpired; })) {
                        return;
                    }
                    if (result == 0) {
                        solver.cancelExecution();
                        } });
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
            std::cout << " (" << ((std::chrono::duration<double>)(end - start)).count() << "," << solver.visitedNodes << "," << (int)solver.hardness << ")";
        std::cout << std::endl;
        // ensure that the solver really has finished otherwise there were some occurences with a segmentation fault, when the instance was deleted, but a task was going to be suspoended 
    }

    return 0;
}
