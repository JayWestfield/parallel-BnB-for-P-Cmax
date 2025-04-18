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
     * @param STtype extended resumable tasks
    */
    BnBSolverBase(bool irrelevance, bool fur, bool gist, bool  addPreviously, int STtype, int maxAllowedParalellism = 6) : irrelevance(irrelevance), gist(gist), fur(fur), addPreviously(addPreviously), STtype(STtype), maxAllowedParalellism(maxAllowedParalellism) {}

    /**
     * @brief Solve P||C_max for the given Instance
     */
    virtual int solve(int numMachines, const std::vector<int>& jobDurations) = 0;

    virtual void cleanUp() = 0;
    std::atomic<uint64_t> visitedNodes = 0;
    std::atomic<bool> foundOptimal = false;
    std::atomic<bool> cancel = false;

    /**
     * @brief describes the relativ difficulty of solving that instance
     */
    enum class Difficulty {
        /**
         * @brief Lpt is the proovable best solution
         */
        trivial,
        /**
         * @brief Lpt is optimal but does not match the lower bound
         */
        lptOpt,
        /**
         * @brief optimal solution is found by the lower Bound
         */
        lowerBoundOptimal,
        /**
         * @brief optimal solution is found by a non trivial lower Bound
         */
        lowerBoundOptimal2,
        /**
         * @brief full search is necessary to gurantee optimum
         */
        full
    };

    Difficulty hardness = Difficulty::lptOpt;
protected:
    /**
     * @brief Reset variables of the base class
     */
    void reset()  {
            visitedNodes = 0;
            foundOptimal = false;
            cancel = false;
            hardness = Difficulty::trivial;
        }
        
    // Settings (Configuration of which rules to use)
    bool irrelevance;
    bool gist;
    bool fur;
    bool addPreviously;
    int STtype;
    int maxAllowedParalellism;


};

#endif