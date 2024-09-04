#ifndef BNB_H
#define BNB_H

#include <vector>
#include <queue>
#include <limits>
#include <tbb/tbb.h>
#include "ST.h"
#include <memory>


class BnBSolver {
public:
    BnBSolver(bool irrelevance, bool gist , bool fur);

    int solve(int numMachines, const std::vector<int>& jobDurations);
    std::atomic<uint64_t> visitedNodes;

private:
    bool irrelevance;
    bool gist;
    bool fur;
    std::unique_ptr<ST> STInstance;
    int lastJobSize;
    int numMachines;
    std::vector<int> jobDurations;
    std::atomic<int> upperBound = std::numeric_limits<int>::max();
    int initialUpperBound = std::numeric_limits<int>::max();
    std::atomic<int> offset = 0;
    bool lookUpGist(std::vector<int>& gist, int job);
    void addGist(std::vector<int>& gist, int job);
    int lowerBound;
    std::vector<std::vector<int>> RET;
    void fillRET();
    void init();
    int computeIrrelevanceIndex();
    int lastRelevantJobIndex;
    int trivialLowerBound();
    int lPTUpperBound();
    bool volatile solveInstance(std::vector<int>& state, int job);
    int left(int i, int u);
    int right(int i, int u);
    void updateBound(int newBound);
    bool lpt(std::vector<int>& state, int job);
    bool lookupRet(int i, int j, int job);
    bool lookupRetFur(int i, int j, int job);
    std::shared_mutex mutex;
    tbb::task_arena arena;
};

#endif // BNB_H
