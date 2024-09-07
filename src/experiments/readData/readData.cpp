#include "readData.h"
#include <fstream>
#include <sstream>

void Parser::readInstance(const std::string &filename, int &numJobs, int &numMachines, std::vector<int> &jobDurations)
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

void Parser::readOptimalSolutions(const std::string &filename, std::unordered_map<std::string, int> &optimalSolutions)
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
