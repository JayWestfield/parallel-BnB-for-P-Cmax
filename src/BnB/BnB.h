#ifndef BNB_H
#define BNB_H

#include <vector>
#include <queue>
#include <limits>
#include <tbb/tbb.h>

class BnBSolver {
public:
    BnBSolver();

    int solve(int numMachines, const std::vector<int>& jobDurations);
    int visitedNodes;

private:


    int numMachines;
    std::vector<int> jobDurations;
    std::atomic<int> upperBound;
    std::atomic<int> offset = 0;

    int lowerBound;
    std::vector<std::vector<int>> RET;
    void fillRET();
    void init();
    void irrelevanceIndex();
    int trivialLowerBound();
    int lPTUpperBound();
    bool solveInstance(const std::vector<int> state, int job);
    int left(int i, int u);
    int right(int i, int u);
};

#endif // BNB_H
