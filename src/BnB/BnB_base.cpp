#ifndef BnB_base_Impl_H
#define BnB_base_Impl_H

#include "ST.h"
#include "STImpl.cpp"
#include <sstream>
#include <stdexcept>
#include "BnB_base.h"
#include <numeric>
#include <tbb/tbb.h>

class BnB_base_Impl : public BnBSolverBase
{
public:
    /**
     * @brief Constructor of a solver.
     *
     * @param irrelevance filter irrelevant jobs
     * @param fur use the fur Rule
     * @param gist take advantage of gists
     * @param addPreviously add the gist (with a flag) when starting to compute it
     */
    BnB_base_Impl(bool irrelevance, bool fur, bool gist, bool addPreviously) : BnBSolverBase(irrelevance, fur, gist, addPreviously) {}
    int solve(int numMachine, const std::vector<int> &jobDuration) override
    {
        reset();
        resetLocals();
        // reset all private variables
        numMachines = numMachine;
        jobDurations = jobDuration;

        // sort since berndt is not Sorted
        std::sort(jobDurations.begin(), jobDurations.end(), std::greater<int>());

        // initialize bounds (trivial nothing fancy here)
        initialUpperBound = lPTUpperBound();
        upperBound = initialUpperBound - 1;
        offset = 1;
        lowerBound = std::max(std::max(jobDurations[0], jobDurations[numMachines - 1] + jobDurations[numMachines]), std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines);

        // irrelevance
        lastRelevantJobIndex = jobDurations.size() - 1;
        if (irrelevance)
            lastRelevantJobIndex = computeIrrelevanceIndex(lowerBound);

        // RET
        offset = 1;
        fillRET();

        // ST for gists
        STInstance = new STImpl(lastRelevantJobIndex + 1, offset, &RET);

        // one JobSize left
        int i = lastRelevantJobIndex;
        while (i > 0 && jobDurations[--i] == jobDurations[lastRelevantJobIndex] )
        {
        }
        lastSizeJobIndex = i + 1;
        if (logInitialBounds)
            std::cout << "Initial Upper Bound: " << upperBound << " Initial lower Bound: " << lowerBound << std::endl;

        // start Computing
        std::vector<int> initialState(numMachines, 0);
        solvePartial(initialState, 0);
        delete STInstance;

        if (cancel)
            return 0;
        // upper Bound is the next best bound so + 1 is the best found bound
        else
            return upperBound + 1;
    }

    void cancelExecution()
    {
        cancel = true;
        if (addPreviously && STInstance != nullptr) STInstance->resumeAllDelayedTasks();
    }

private:
    // Problem instance
    int numMachines;
    std::vector<int> jobDurations;

    // Bounds
    std::atomic<int> upperBound;
    std::atomic<int> lowerBound;
    int initialUpperBound;
    std::shared_mutex boundLock;

    // RET
    std::vector<std::vector<int>> RET = {{}};
    std::atomic<int> offset;

    // gist
    STImpl *STInstance = nullptr;

    // Logging
    bool logNodes = false;
    bool logInitialBounds = false;
    bool logBound = false;
    bool detailedLogging = false;

    int lastRelevantJobIndex;

    // only one jobsize left
    int lastSizeJobIndex;
    int trivialLowerBound()
    {
        return std::max(std::max(jobDurations[0], jobDurations[numMachines - 1] + jobDurations[numMachines]), std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines);
    }

    void solvePartial(std::vector<int> state, int job)
    {

        if (foundOptimal || cancel)
            return;

        std::sort(state.begin(), state.end()); // currently for simplification
        int makespan = state[state.size() - 1];

        visitedNodes++;
        if (logNodes && visitedNodes % 1000000 == 0)
        {
            std::cout << "visited nodes: " << visitedNodes << " current Bound: " << upperBound << " " << job << " makespan " << makespan << std::endl;
            std::cout << "current state: ";
            for (int i : state)
                std::cout << " " << i;
            std::cout << std::endl;
        }

        // no further recursion necessary?
        if (job > lastRelevantJobIndex)
        {
            updateBound(makespan);
            return;
        }
        else if (makespan > upperBound)
            return;

        // 3 jobs remaining
        else if (job == lastRelevantJobIndex - 2)
        {
            std::vector<int> one = state;
            lpt(one, job);
            std::vector<int> two = state;
            two[1] += jobDurations[job++];
            lpt(two, job);
            return;
        }
        // Rule 5
        else if (job >= lastSizeJobIndex)
        { // Rule 5
            // TODO it should be able to do this faster (at least for many jobs with the same size) by keeping the state in a priority queu or sth like that
            // TODO there is a contition whioch says, wether that instance can be solved
            lpt(state, job);
            return;
        }
        logging(state, job, "before lookup");

        // Gist Lookup
        if (gist)
        {
            int exists;
            try
            {
                exists = STInstance->exists(state, job);
            }
            catch (const std::runtime_error &e)
            {
                return;
            }
            logging(state, job, exists);
            int bound = upperBound;
            if (exists == 2)
                return;
            else if (exists == 1)
            {
                while (STInstance->exists(state, job) == 1)
                { // TODO try catch for the whole block
                    logging(state, job, "suspend");
                    oneapi::tbb::task::suspend([=](oneapi::tbb::task::suspend_point tag)
                                               {
                                                   STInstance->addDelayed(state, job, tag);
                                                   // oneapi::tbb::task::resume(tag);
                                               });
                    logging(state, job, "restarted");
                    // invariant a task is only resumed when the corresponding gist is added, or when the bound was updated therefore one can return if the bound has not updated since bound == upperBound ||
                    if (makespan > upperBound || foundOptimal || cancel)
                        return;
                }
                if (STInstance->exists(state, job) == 2)
                    return;
            }
            else if (exists == 0 && addPreviously)
            try {
                STInstance->addPreviously(state, job);
            }
                            catch (const std::runtime_error &e)
                            {
                                return;
                            }
        }

        // FUR
        if (fur)
        {
            for (int i = (state.size() - 1); i >= 0; i--)
            {
                if (state[i] + jobDurations[job] <= upperBound && lookupRetFur(state[i], jobDurations[job], job))
                {
                    std::vector<int> next = state; // when implemented carefully there is no copy needed (but we are not that far yet)
                    next[i] += jobDurations[job];
                    solvePartial(next, job + 1);
                    if (state[i] + jobDurations[job] <= upperBound)
                    {
                        if (gist)
                        {
                            try
                            {
                                STInstance->addGist(state, job);
                            }
                            catch (const std::runtime_error &e)
                            {
                                return;
                            }
                        }
                        logging(state, job, "after add");
                        return;
                    }
                }
            }
        }
        logging(state, job, "after Fur");

        tbb::task_group tg;
        int endState = state.size();
        std::vector<int> repeat;
        if (numMachines > lastRelevantJobIndex - job + 1)
            endState = lastRelevantJobIndex - job + 1; // Rule 4 only usefull for more than 3 states otherwise rule 3 gets triggered

        assert(endState <= state.size());
        for (int i = 0; i < endState; i++)
        {
            if ((i > 0 && state[i] == state[i - 1]) || state[i] + jobDurations[job] > upperBound)
                continue; // Rule 1 + check directly wether the new state would be feasible
            if (i > 0 && lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                repeat.push_back(i);
                continue;
            }
            std::vector<int> next = state;
            next[i] += jobDurations[job];
            logging(next, job + 1, "child from recursion");
            tg.run([=, &tg]
                   { solvePartial(next, job + 1); });
        }
        logging(state, job, "wait for tasks to finish");
        tg.wait();
        logging(state, job, "after recursion");
        for (int i : repeat)
        {
            if (!lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                std::vector<int> next = state;
                next[i] += jobDurations[job];
                logging(next, job + 1, "Rule 6 from recursion");
                tg.run([=, &tg]
                       { solvePartial(next, job + 1); });
            }
        }
        tg.wait();

        std::sort(state.begin(), state.end());
        if (gist)
        {
            try
            {
                STInstance->addGist(state, job);
            }
            catch (const std::runtime_error &e)
            {
                return;
            }
        }
        logging(state, job, "after add");
        return;
    }

    void logging(std::vector<int> state, int job, auto message = "")
    {
        if (!detailedLogging)
            return;
        std::stringstream gis;
        gis << message << " ";
        for (auto vla : state)
            gis << vla << " ";
        gis << " => ";
        for (auto vla : STInstance->computeGist(state, job))
            gis << vla << " ";
        gis << " Job: " << job << "\n";
        std::cout << gis.str() << std::endl;
    }

    bool lookupRet(int i, int j, int job)
    {
        std::shared_lock lock(boundLock); // Depending on the bound update we can remove the lock
        const int off = offset;
        assert(i + off < RET[job].size());
        return RET[job][i + off] == RET[job][j + off];
    }
    bool lookupRetFur(int i, int j, int job)
    { // this is awful codestyle needs to be refactored
        std::shared_lock lock(boundLock);
        assert(i + offset < RET[job].size() && initialUpperBound - j < RET[job].size());
        return RET[job][i + offset] == RET[job][initialUpperBound - j]; // need to make sure that upperbound and offset are correct done with initialBound (suboptimal)
    }

    /**
     * @brief runs Lpt on the given partialassignment
     */
    void lpt(std::vector<int> state, int job)
    {
        for (long unsigned int i = job; i < jobDurations.size(); i++)
        {
            int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
            assert(minLoadMachine >= 0 && minLoadMachine < state.size());
            state[minLoadMachine] += jobDurations[i];
        }
        int makespan = *std::max_element(state.begin(), state.end());
        if (makespan <= upperBound)
        {
            updateBound(makespan);
        }
        return;
    }
    /**
     * @brief tightens the upper bound and updates the offset for the RET
     */
    void updateBound(int newBound)
    {
        if (newBound == lowerBound)
        {
            foundOptimal = true;
        }
        if (logBound)
            std::cout << "try Bound " << newBound << std::endl;

        std::unique_lock lock(boundLock); // Todo this can be done with CAS ( i think)
        if (newBound > upperBound)
            return;
        if (logBound)
            std::cout << "new Bound " << newBound << std::endl;
        upperBound = newBound - 1;
        offset = initialUpperBound - upperBound;
        if (gist)
            STInstance->boundUpdate(offset);
        if (logBound)
            std::cout << "new Bound " << newBound << "finished" << std::endl;
        lock.unlock();
    }

    /**
     * @brief compute an upper bound via LPT heuristic
     */
    int lPTUpperBound()
    {
        std::vector<int> upper(numMachines, 0);
        for (long unsigned int i = 0; i < jobDurations.size(); i++)
        {
            int minLoadMachine = std::min_element(upper.begin(), upper.end()) - upper.begin();
            assert(minLoadMachine >= 0 && minLoadMachine < upper.size());
            upper[minLoadMachine] += jobDurations[i];
        }
        return *std::max_element(upper.begin(), upper.end());
    }

    /**
     * @brief find index, to when jobs are no longer relevant regarding the given Bound.
     */
    int computeIrrelevanceIndex(int bound)
    {
        /*std::vector<int> partialSum(jobDurations.size() + 1);
        std::exclusive_scan(jobDurations.begin(), jobDurations.end(), partialSum.begin(), 0);
        int i = jobDurations.size() - 1;
        while ( i > 0 && partialSum[i] < numMachines * (bound - jobDurations[i] + 1)) i--;
        return i; */

        std::vector<int> sum(jobDurations.size() + 1);
        sum[0] = 0;

        std::partial_sum(jobDurations.begin(), jobDurations.end(), sum.begin() + 1);
        int i = jobDurations.size() - 1;
        // TODO think: do i need to update the lowerBound then aswell? no i dont think so
        while (i > 0 && sum[i] < numMachines * (lowerBound - jobDurations[i] + 1))
        { // TODO think about that do i still need to add them in the end right?
            i--;
        }

        return i;
    }

    /**
     * @brief initializes and fills the RET
     */
    void fillRET()
    {
        auto left = [this](int i, int u)
        {
            return RET[i + 1][u];
        };
        auto right = [this](int i, int u)
        {
            if (u + jobDurations[i] > initialUpperBound)
                return 0;
            return RET[i + 1][u + jobDurations[i]];
        };
        RET = std::vector<std::vector<int>>(lastRelevantJobIndex + 1, std::vector<int>(initialUpperBound + 1));
        for (auto u = 0; u <= initialUpperBound; u++)
        { // TODO there is some point u where everithing before is a 1 and after it is a 2 so 2 for loops could be less branches
            if (u + jobDurations[lastRelevantJobIndex] > initialUpperBound)
            {
                RET[lastRelevantJobIndex][u] = 1;
            }
            else
            {
                RET[lastRelevantJobIndex][u] = 2;
            }
        }
        for (auto i = 0; i <= lastRelevantJobIndex; i++)
        {
            RET[i][initialUpperBound] = 1;
        }
        for (auto i = lastRelevantJobIndex - 1; i >= 0; i--)
        {
            for (int u = initialUpperBound - 1; u >= 0; u--)
            {
                if (left(i, u) == left(i, u + 1) && right(i, u) == right(i, u + 1))
                {
                    RET[i][u] = RET[i][u + 1];
                }
                else
                {
                    RET[i][u] = RET[i][u + 1] + 1;
                }
            }
        }
        /*std::cout << "Logging RET:\n";
        for (size_t i = 0; i < RET.size(); ++i) {
            for (size_t j = 0; j < RET[i].size(); ++j) {
                std::cout << RET[i][j] << " ";
            }
            std::cout << "\n";
        }
        std::cout << "End of RET log\n\n"; */
    }

    void resetLocals()
    {
        numMachines = 0;
        jobDurations.clear();
        upperBound = 0;
        lowerBound = 0;
        initialUpperBound = 0;
        offset = 0;
        lastRelevantJobIndex = 0;
        lastSizeJobIndex = 0;
        cancel = false;
        foundOptimal = false;
        visitedNodes = 0;
        RET.clear();
    }
};

#endif