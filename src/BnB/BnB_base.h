#ifndef BNB_base_H
#define BNB_base_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>
class BnBSolverBase {
public:
    /**
     * @brief Constructor of a solver.
     *
     * @param irrelevance filter irrelevant jobs
     * @param fur use the fur Rule
     * @param gist take advantage of gists
     * @param addPreviously add the gist (with a flag) when starting to compute it
    */
    BnBSolverBase(bool irrelevance, bool fur, bool gist, bool  addPreviously) : irrelevance(irrelevance), gist(gist), fur(fur), addPreviously(addPreviously) {}

    /**
     * @brief Solve P||C_max for the given Instance
     */
    virtual int solve(int numMachines, const std::vector<int>& jobDurations) = 0;

    std::atomic<uint64_t> visitedNodes = 0;
    bool foundOptimal = false;
    bool cancel = false;
protected:
    /**
     * @brief Reset variables of the base class
     */
    void reset()  {
            visitedNodes = 0;
            foundOptimal = false;
            cancel = false;
        }
        
    // Settings (Configuration of which rules to use)
    bool irrelevance;
    bool gist;
    bool fur;
    bool addPreviously;

};

#endif