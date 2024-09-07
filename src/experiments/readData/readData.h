
#ifndef Parser_H
#define Parser_H

#include <string>
#include <vector>
#include <unordered_map>
class Parser
{
    public:
        Parser(){};

        void readInstance(const std::string &filename, int &numJobs, int &numMachines, std::vector<int> &jobDurations);
        void readOptimalSolutions(const std::string &filename, std::unordered_map<std::string, int> &optimalSolutions);

};
#endif