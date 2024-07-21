#include <iostream>
#include "BnB/BnB.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <tbb/tbb.h> 
void readInstance(const std::string& filename, int& numJobs, int& numMachines, std::vector<int>& jobDurations) {
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
void readOptimalSolutions(const std::string& filename, std::unordered_map<std::string, int>& optimalSolutions) {
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

int main() {
    

    try {
        std::vector<int> jobDurations;
        std::unordered_map<std::string, int> optimalSolutions;
        std::vector<std::string> failed;
        std::vector<std::string> succesfull;
        BnBSolver solver(true, true, true);
        
    std::vector<int> plantedKnownOptimal={699 ,461 ,249 ,207 ,746 ,270 ,790 ,514 ,498, 682};
    int result = solver.solve(2, plantedKnownOptimal);

    std::cout << "The optimal solution is: " << 2563 << std::endl;
    std::cout << "The best found solution is: " << result << std::endl;
    std::cout << "nodes visited: " << solver.visitedNodes << std::endl;
         std::vector<int> plantedKnownOptimal2={117,117,115,114,101,95,95,84,78,76,75,75,71,67,66,60,54,53,52,52,51,50,48,46,43,43,42,41,41,40,39,38,38,37,35,34,33,29,28,28,27,26,26,24,22,22,22,18,17,17,16,15,15,15,15,15,14,14,13,13,12,12,12,11,9,9,9,9,9,9,8,8,8,8,7,7,7,7,6,6,6,6,5,5,5,5,4,4,4,4,4,4,3,2,2,2,2,1,1,1,0
        };
             result = solver.solve(10, plantedKnownOptimal2);

            std::cout << "The optimal solution is: " << 301 << std::endl;
            std::cout << "The best found solution is: " << result << std::endl;
            std::cout << "With nodes visited: " << solver.visitedNodes << std::endl;

        // Read the known optimal solutions
        std::string optimalSolutionsFile = "benchmarks/opt-known-instances-berndt.txt";

        readOptimalSolutions(optimalSolutionsFile, optimalSolutions);
        int testInstances = 150;//optimalSolutions.size();
        const int excludeLast = 0;
        auto it = optimalSolutions.begin();
        for (int s = 0; s < excludeLast; s++) {
            it++;
        }
        std::vector<std::vector<double>> speedups;
        for (int i = 0; i < testInstances; i++) {
            const auto instanceFile = it->first;
            const auto knownOptimal = it++->second;

            int numJobs, numMachines;
            std::vector<int> jobDurations;

            std::string instanceFilePath = "benchmarks/berndt/" + instanceFile;
            readInstance(instanceFilePath, numJobs, numMachines, jobDurations);

            std::vector<std::chrono::duration<double>> runtimes;
            std::vector<int> results;
            std::cout << "Instance: " << instanceFile << std::endl;
            for (int numThreads:  {1, 2, 4, 8, 12}) { // i have locally "only" 12 threads 
                //std::cout << "Start try with: " << numThreads << std::endl; 
                tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, numThreads);                
                auto start = std::chrono::high_resolution_clock::now();
                int result = solver.solve(numMachines, jobDurations);
                auto end = std::chrono::high_resolution_clock::now();
                results.push_back(result);
                runtimes.push_back( end - start);
            }
            std::cout << "runtimes: ";
            for (auto time : runtimes) {
                std::cout << time.count() <<  " ";
            }
            std::cout << std::endl << "speedup: "; // todo safe speedups and return the biggest and smalles in the end
            std::vector<double> ewst;
            speedups.push_back(ewst);
            for (auto time : runtimes) {
                speedups[speedups.size() -1].push_back(runtimes[0].count() / time.count()); 
                std::cout << runtimes[0].count() / time.count() <<  " ";
            }


            std::cout << std::endl << "Calculated makespan: " << results[0] << std::endl;
            std::cout << "Known optimal makespan: " << knownOptimal << std::endl;
            std::cout << "visited Nodes: " << solver.visitedNodes << std::endl;
            
            if (results[0] == knownOptimal) { // TODO curretnly onnly the last resolt with 12 threads is checked the others are ignored
                succesfull.push_back("" + instanceFilePath);
                std::cout << "Success: Found the optimal makespan with "<<  solver.visitedNodes << " visited nodes on the last try"<< std::endl;
            } else {
                failed.push_back(instanceFilePath);
                std::cout << "Error: The calculated makespan does not match the known optimal value." << std::endl;
            }
            std::cout << "--------------------------------------------------------------------------------------------------------" << std::endl;
        }
        if (testInstances > 0) {
            std::cout << "Failed instances: " <<  failed.size() << "/" << failed.size() + succesfull.size() << " = " << (double) failed.size() * 100 / (double) (failed.size() + succesfull.size()) << "%" << std::endl;
            if (failed.size() > 0) std::cout << "Failed instances: " << std::endl;
            for (auto inst : failed) {
                std::cout << inst << std::endl;
            }
            std::vector<double> best(5,0);
            std::vector<double> worst(5,7);
            for (auto speedup : speedups) {
                if (speedup[speedup.size() - 1] > best[4] ) best = speedup;
                if (speedup[speedup.size() - 1] < worst[4] ) worst = speedup;
            }  
            std::cout << "worst found Speedups: ";
            for (auto time : worst) std::cout << time << " ";

            std::cout << std::endl <<  "best found Speedups: ";
            for (auto time : best) std::cout << time << " ";
            std::cout << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
