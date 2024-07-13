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
    std::atomic<int> visitedNodes;

private:

    int lastJobSize;
    int numMachines;
    std::vector<int> jobDurations;
    std::atomic<int> upperBound = std::numeric_limits<int>::max();
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
    void updateBound(int newBound);
    bool lpt(std::vector<int> state, int job);
};

#endif // BNB_H
