#include <vector>
#include <queue>
#include <algorithm>
#include <numeric>
#include <climits>
#include <iostream>
#include <atomic>
#include "lowerBounds.h"
#include <cassert>
#include <sstream>

class lowerBounds_ret : public LowerBound_base
{
public:
    lowerBounds_ret(std::atomic<bool> &foundOptimal, std::atomic<bool> &cancel)
        : LowerBound_base(foundOptimal, cancel) {}

    int solve(const std::vector<int> &jobs, int numMachine)
    {
        this->numMachines = numMachine;
        this->jobDurations = jobs;
        // init();
        offset = 1;
        initialUpperBound = lPTUpperBound();
        upperBound = initialUpperBound - 1;
        lowerBound = trivialLowerBound();

        if (initialUpperBound == lowerBound)
        {
            return initialUpperBound;
        }

        lastRelevantJobIndex = jobDurations.size() - 1;
        lastRelevantJobIndex = computeIrrelevanceIndex(lowerBound);

        // RET
        offset = 1;
        fillRET();
        int i = jobs.size() - 1;
        while (i > 0 && jobDurations[--i] == jobDurations[jobs.size() - 1])
        {
        }
        lastSizeJobIndex = i;

        std::vector<int> initialState(numMachines, 0);
        solvePartial(initialState, 0);
        if ( cancel || foundOptimal) return 0;
        return upperBound + 1;
    }
    // TODO check whole file wether upper bound is the last found best solution or one below
private:
    // Problem instance
    int numMachines;
    std::vector<int> jobDurations;

    // Bounds
    int upperBound;
    int lowerBound;
    int initialUpperBound;
    // RET
    std::vector<std::vector<int>> RET = {{}};
    int offset;
    int lastSizeJobIndex;
    int lastRelevantJobIndex;

    bool solvePartial(std::vector<int> state, int job)
    {
        assert(std::is_sorted(state.begin(), state.end()));

        int makespan = state[numMachines - 1];
        if (foundOptimal || cancel || makespan > upperBound)
            return false;


        if (job > lastRelevantJobIndex)
        {
            if (makespan <= upperBound)
            {
                upperBound = makespan - 1;
                offset = initialUpperBound - upperBound;
                return true;
            }
            else
                return false;
        }
        if (job >= lastSizeJobIndex)
        {
            return lpt(state, job);
        }

        // Fur
        for (int i = (numMachines - 1); i >= 0; i--)
        {
            if (false && state[i] + jobDurations[job] <= upperBound && state[i] + jobDurations[job] >= 99 && lookupRetFur(state[i], jobDurations[job], job))
            {
                auto next = state;
                assert(next == state);
                // when implemented carefully there is no copy needed (but we are not that far yet)
                next[i] += jobDurations[job];
                // std::sort(next.begin(), next.end());
                resortAfterIncrement(next, i);
                bool solved = solvePartial(next, job + 1);
                if (solved)
                {
                    if (state[i] + jobDurations[job] <= upperBound && lookupRetFur(state[i], jobDurations[job], job))
                    {
                        return true;
                    }
                }
                else
                {
                    return false;
                }
            }
        }
        // TODO rule 4 (uncecessary if last jobs get all the same size)
        // TODO baseCase with some other rules
        int endState = numMachines;
        if (numMachines > jobDurations.size() - job + 1)
            endState = jobDurations.size() - job + 1; // Rule 4 only usefull for more than 3 states otherwise rule 3 gets triggered
        std::vector<int> repeat;
        bool improvement = false;
        for (int i = 0; i < endState; i++)
        {
            if ((i > 0 && state[i] == state[i - 1]) || state[i] + jobDurations[job] > upperBound)
                continue; // Rule 1 + check directly wether the new state would be feasible
            if (i > 0 && lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                repeat.push_back(i);
                continue;
            }
            auto next = state;
            (next)[i] += jobDurations[job];

            // std::sort(next.begin(), next.end());
            resortAfterIncrement(next, i);
            // !!!  if you use |= the compiler will not execule all of it
            bool isImprove = solvePartial(next, job + 1);
            improvement = isImprove || improvement;
        }
        for (int i : repeat)
        {
            if (!lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                auto next = state;
                next[i] += jobDurations[job];
                resortAfterIncrement(next, i);
                bool isImprove = solvePartial(next, job + 1);
                improvement = isImprove || improvement;
            }
        }
        return true;
    }
    inline int trivialLowerBound()
    {
        return std::max(std::max(jobDurations[0], jobDurations[numMachines - 1] + jobDurations[numMachines]), std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines);
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
            assert(minLoadMachine >= 0 && (std::size_t)minLoadMachine < upper.size());
            upper[minLoadMachine] += jobDurations[i];
        }
        return *std::max_element(upper.begin(), upper.end());
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
    }
    /**
     * @brief runs Lpt on the given partialassignment
     */
    bool lpt(std::vector<int> &state, int job)
    {
        for (long unsigned int i = job; i < jobDurations.size(); i++)
        {
            int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
            assert(minLoadMachine >= 0 && minLoadMachine < numMachines);
            state[minLoadMachine] += jobDurations[i];
        }
        int makespan = *std::max_element(state.begin(), state.end());
        if (makespan <= upperBound)
        {
            upperBound = makespan - 1;
            offset = initialUpperBound - upperBound;
            return true;
        }
        else
            return false;
    }
    inline bool lookupRet(int i, int j, int job)
    {
        assert(i + offset < (int)RET[job].size() && j + offset < (int)RET[job].size());
        return RET[job][i + offset] == RET[job][j + offset];
    }
    inline bool lookupRetFur(int i, int j, int job)
    {
        assert(i + offset < (int)RET[job].size() && initialUpperBound - j < (int)RET[job].size());
        return RET[job][i + offset] == RET[job][initialUpperBound - j]; // need to make sure that upperbound and offset are correct done with initialBound (suboptimal)
    }

    // basically insertion sort
    inline void resortAfterIncrement(std::vector<int> &vec, size_t index)
    {
        assert(index < vec.size());
        int incrementedValue = vec[index];
        // since it was an increment the newPosIt can only be at the same index or after
        auto newPosIt = std::upper_bound(vec.begin() + index, vec.end(), incrementedValue);
        size_t newPosIndex = std::distance(vec.begin(), newPosIt);
        if (newPosIndex == index)
        {
            return;
        }
        std::rotate(vec.begin() + index, vec.begin() + index + 1, vec.begin() + newPosIndex);
        assert(std::is_sorted(vec.begin(), vec.end()));
    }

    /**
     * @brief find index, to when jobs are no longer relevant regarding the given Bound.
     */
    int computeIrrelevanceIndex(int bound)
    {
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
    template <typename T>
    inline void logging(const std::vector<int> &state, int job, T message = "")
    {

        std::stringstream gis;
        gis << message << " ";
        for (auto vla : state)
            gis << vla << ", ";
        gis << " => ";
        // for (auto vla : STInstance->computeGist(state, job))
        //     gis << vla << " ";
        gis << " Job: " << job;
        std::cout << gis.str() << std::endl;
    }
};
