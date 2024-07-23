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
        //tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, 1);                
    for (int i = 0 ; i < 1; i++) {
    std::vector<int> plantedKnownOptimal={92,69,26,126,180,118,79,47,36,118,65,113,82,137,65,49,103,169,8,147,0};
    int result = solver.solve(8,plantedKnownOptimal); 
    std::cout << result << std::endl;// really weird error if this is not 231 then there will be error instances so it has to be an missing initializer or sth like that i have no idea
    if (result != 231) {
        std::cout << "Error This would be a false run " << std::endl;
        return 0;
    }


     }    
     
    std::cout << "The optimal solution is: " << 231 << std::endl; // really weird error if this is 
    //std::cout << "The best found solution is: " << result << std::endl;
    std::cout << "nodes visited: " << solver.visitedNodes << std::endl;
        // Read the known optimal solutions
        std::string optimalSolutionsFile = "benchmarks/opt-known-instances-lawrinenko.txt";

        readOptimalSolutions(optimalSolutionsFile, optimalSolutions);
        int testInstances = optimalSolutions.size();
        const int excludeLast = 0;
        auto it = optimalSolutions.begin();
        for (int s = 0; s < excludeLast; s++) {
            it++;
        }
        std::vector<std::vector<double>> speedups;
        for (int i = 0; i < testInstances; i++) {
            const auto instanceFile = it->first;
            const auto knownOptimal = it++->second;
            if (instanceFile.find("n22-") == -1) continue; // only test specific instances

            int numJobs, numMachines;
            std::vector<int> jobDurations;

            std::string instanceFilePath = "benchmarks/lawrinenko/" + instanceFile;
            readInstance(instanceFilePath, numJobs, numMachines, jobDurations);

            std::vector<std::chrono::duration<double>> runtimes;
            std::vector<int> results;
            std::vector<int> visitedNodes;
            std::cout << "Instance: " << instanceFile << std::endl;
            for (int numThreads:  {1, 2, 4, 8, 12}) { // i have locally "only" 12 threads 
                //std::cout << "Start try with: " << numThreads << std::endl; 
                tbb::global_control global_limit(tbb::global_control::max_allowed_parallelism, numThreads);                
                auto start = std::chrono::high_resolution_clock::now();
                int result = solver.solve(numMachines, jobDurations);
                auto end = std::chrono::high_resolution_clock::now();
                results.push_back(result);
                runtimes.push_back( end - start);
                visitedNodes.push_back(solver.visitedNodes);
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
            std::cout <<std::endl <<  "Visited Nodes : ";
            for (auto visited : visitedNodes) {
                std::cout << visited <<  " ";
            }
            std::cout <<std::endl <<  "increase Nodes: ";
            for (auto visited : visitedNodes) {
                std::cout << (double) visited / (double) visitedNodes[0]  <<  " ";
            }
            std::cout <<std::endl;
           
            if (results[4] == knownOptimal) { // TODO curretnly onnly the last resolt with 12 threads is checked the others are ignored
                succesfull.push_back("" + instanceFilePath);
                std::cout << "Success: Found the optimal makespan with "<<  solver.visitedNodes << " visited nodes on the last try"<< std::endl;
            } else {
                failed.push_back(instanceFilePath);
                std::cout << "Error: The calculated makespan of " <<results[4] <<  " does not match the known optimal value of " << knownOptimal << std::endl;
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
            std::vector<double> average(5,0);
            int speedupInstances = 0;
            int speedupInstancesWeak = 0;
            for (auto speedup : speedups) {
                if (speedup[speedup.size() - 1] > best[4] ) best = speedup;
                if (speedup[speedup.size() - 1] < worst[4] ) worst = speedup;
                for (int i = 0; i < average.size(); i++) average[i] += speedup[i];
                bool better = true;
                for (int i = 1; i < speedup.size(); i++) better &= (speedup[i] > 1.0002);
                if (better) speedupInstances++;
                better = false;
                for (int i = 1; i < speedup.size(); i++) better |= (speedup[i] > 1);
                if (better) speedupInstancesWeak++;
             }  
            for (int i = 0; i < average.size(); i++) average[i] = average[i] / speedups.size();

            std::cout << "worst found Speedups: ";
            for (auto time : worst) std::cout << time << " ";

            std::cout << std::endl <<  "best found Speedups: ";
            for (auto time : best) std::cout << time << " ";

            std::cout << std::endl << "average found Speedups: ";
            for (auto time : average) std::cout << time << " ";

            std::cout << std::endl << "Instances with a speedup for all threads: ";
            std::cout << ((double) speedupInstances / speedups.size()) * 100 << "%";

            std::cout << std::endl << "Instances with a speedup for at least one threads: ";
            std::cout << ((double) speedupInstancesWeak / speedups.size()) * 100 << "%";
            std::cout << std::endl;

        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
