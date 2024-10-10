#ifndef BnB_base_Impl_H
#define BnB_base_Impl_H

#include "STImpl.cpp"
#include "STImplSimplCustomLock.cpp"
#include "LowerBounds/lowerBounds_ret.cpp"
#include <chrono>
#include <sstream>
#include <stdexcept>
#include "BnB_base.h"
#include <numeric>
#include <tbb/tbb.h>
#include <condition_variable>
#include <thread>
const int limitedTaskSpawning = 2000;
class BnB_base_Impl : public BnBSolverBase
{
public:

    std::vector<std::chrono::duration<double>> timeFrames;
    std::chrono::_V2::system_clock::time_point lastUpdate;
    /**
     * @brief Constructor of a solver.
     *
     * @param irrelevance filter irrelevant jobs
     * @param fur use the fur Rule
     * @param gist take advantage of gists
     * @param addPreviously add the gist (with a flag) when starting to compute it
     * @param STtype specifies the STType
     */
    BnB_base_Impl(bool irrelevance, bool fur, bool gist, bool addPreviously, int STtype) : BnBSolverBase(irrelevance, fur, gist, addPreviously, STtype) {}
    int solve(int numMachine, const std::vector<int> &jobDuration) override
    {
        reset();
        resetLocals();
        // reset all private variables
        numMachines = numMachine;
        jobDurations = jobDuration;
        tbb::task_group tg;

        // assume sorted
        assert(std::is_sorted(jobDurations.begin(), jobDurations.end(), std::greater<int>()));
        offset = 1;
        tbb::task_group firstbounds;
        firstbounds.run([&]
                        { initialLowerBound = trivialLowerBound(); 
                        lowerBound = initialLowerBound; });

        firstbounds.run([&]
                        {
            // initialize bounds (trivial nothing fancy here)
            initialUpperBound = lPTUpperBound();
            upperBound = initialUpperBound - 1; });
        firstbounds.wait();
        if (initialUpperBound == lowerBound)
        {
            hardness = Difficulty::trivial;
            return initialUpperBound;
        }

        tbb::task_group tg_lower;

        // irrelevance
        lastRelevantJobIndex = jobDurations.size() - 1;
        if (irrelevance)
            lastRelevantJobIndex = computeIrrelevanceIndex(lowerBound);

        tg_lower.run([this]
                     { improveLowerBound(); });
        // RET
        offset = 1;
        fillRET();

        // ST for gists
        initializeST();
        // one JobSize left
        int i = lastRelevantJobIndex;
        // TODO last size can be done async
        while (i > 0 && jobDurations[--i] == jobDurations[lastRelevantJobIndex])
        {
        }
        lastSizeJobIndex = i + 1;
        if (logInitialBounds)
            std::cout << "Initial Upper Bound: " << upperBound << " Initial lower Bound: " << lowerBound << std::endl;

        lastUpdate = std::chrono::high_resolution_clock::now();
        // start Computing
        std::vector<int> initialState(numMachines, 0);
        auto start = std::chrono::high_resolution_clock::now();
        tg.run([this, initialState]
               { solvePartial(initialState, 0); });
        std::condition_variable mycond;
        std::mutex mtx;

        std::thread monitoringThread([&](){
            while ( !foundOptimal && !cancel)
                {
                // std::cout << "trigger mem and resume " <<  foundOptimal << std::endl;
                // STInstance->resumeAllDelayedTasks();
                double memoryUsage = getMemoryUsagePercentage();
                if (memoryUsage > 85)
                    {
                        std::cout << "Memory usage high: " << memoryUsage << "%. Calling evictAll. "  << ( (std::chrono::duration<double>)(std::chrono::high_resolution_clock::now() - start)).count()<< std::endl;
                        std::unique_lock lock(boundLock);
                        STInstance->clear();

                        memoryUsage = getMemoryUsagePercentage();
                        std::cout << "Memory usage after evictAll: " << memoryUsage << "%" << std::endl;
                    } 
                    // interuptable wait
                    std::unique_lock<std::mutex> condLock(mtx);
                    mycond.wait_for( condLock, std::chrono::milliseconds(500));
                } 
            return; 
        });
        tg.wait();

        timeFrames.push_back((std::chrono::high_resolution_clock::now() - lastUpdate));

        if (cancel)
        {
            tg_lower.wait();
            return 0;
        }

        foundOptimal = true;
        mycond.notify_all();


        // upper Bound is the next best bound so + 1 is the best found bound
        if (initialUpperBound == upperBound + 1)
        {
            hardness = Difficulty::lptOpt;
        }
        else if (upperBound + 1 == lowerBound)
        {
            if (lowerBound != initialLowerBound)
                hardness = Difficulty::lowerBoundOptimal2;
            else
                hardness = Difficulty::lowerBoundOptimal;
        }
        else
        {
            hardness = Difficulty::full;
        }

        monitoringThread.join();
        tg_lower.wait();

        return upperBound + 1;
    }

    void getSubInstance(std::vector<int> &sub, int jobSize)
    {
        for (int i = 0; i < jobSize; i++)
        {
            sub[i] = jobDurations[i];
        }
        if (fillSubinstances)
        {
            int fillTo = lastRelevantJobIndex;
            int fillSize = jobDurations[lastRelevantJobIndex]; // TODO find a better way 
            if (jobSize < lastRelevantJobIndex - 5) {
                fillTo -= 5;
                fillSize = jobDurations[fillTo];
            }
            for (int i = jobSize; i < fillTo; i++)
            {
                sub[i] = fillSize;
            }
            sub.resize(fillTo);
        }
        else
        {
            sub.resize(jobSize);
        }
    }
    void improveLowerBound()
    {
        int jobSize = (double)lastRelevantJobIndex * 0.7; // TODO so setzen, dass ein gewisser prozentteil des gesamtgewichtes drin ist
        std::vector<int> sub(lastRelevantJobIndex);
        while (!cancel && !foundOptimal)
        {
            lowerBounds_ret lowerSolver(foundOptimal, cancel);
            getSubInstance(sub, jobSize);
            int solvedSubinstance = lowerSolver.solve(sub, numMachines);
            if (solvedSubinstance > lowerBound)
                lowerBound = solvedSubinstance;
            if (lowerBound == upperBound + 1)
            {
                foundOptimal = true;
            }
            if (lowerBound > upperBound + 1) // TODO that should be an assert
            {
                throw std::runtime_error("Subinstance found a lower bound " + std::to_string(lowerBound) + " that is higher than the current UpperBound " + std::to_string(upperBound));
            }
            jobSize += 4;
            if (jobSize >= lastRelevantJobIndex)
                break;
        }
    }

    void cleanUp() override
    {
        reset();
        resetLocals();
    }
    void cancelExecution()
    {
        cancel = true;
        if (STInstance != nullptr)
            STInstance->resumeAllDelayedTasks();
    }

private:
    // Problem instance
    int numMachines;
    std::vector<int> jobDurations;

    // Bounds
    std::atomic<int> upperBound;
    int initialLowerBound;
    std::atomic<int> lowerBound;
    int initialUpperBound;
    std::mutex boundLock;

    // RET
    std::vector<std::vector<int>> RET = {{}};
    std::atomic<int> offset;

    // ST
    ST *STInstance = nullptr;

    // Logging
    bool logNodes = false;
    bool logInitialBounds = false;
    bool logBound = false;
    bool detailedLogging = true;

    int lastRelevantJobIndex;

    // solve Subinstances
    bool useSubinstances = true;
    bool fillSubinstances = false;
    bool adaptiveSubinstances = true; // experimental for the future do sth. like depending on the variety of jobsizes do fill subinstances or not or adapt the size of the subinstance

    // for logging the amount of tasks that get executed even though the gist is currently computed
    std::atomic<int> runWithPrev = 0;
    std::atomic<int> allPrev = 0;

    // only one jobsize left
    int lastSizeJobIndex;
    inline int trivialLowerBound()
    {
        return std::max(std::max(jobDurations[0], jobDurations[numMachines - 1] + jobDurations[numMachines]), std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines);
    }

    void solvePartial(const std::vector<int> &state, int job)
    {

        assert(std::is_sorted(state.begin(), state.end()));
        if (foundOptimal || cancel)
            return;

        int makespan = state[numMachines - 1];

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
            // TODO there is a condition which says, wether that instance can be solved that might be faster than running lpt
            std::vector<int> one = state;
            lpt(one, job);
            return;
        }
        logging(state, job, "before lookup");

        if (gist)
        {
            int exists = STInstance->exists(state, job);

            logging(state, job, exists);
            switch (exists)
            {
            case 2:
                logging(state, job, "gist found");
                return;
            case 1:
                if (addPreviously && job <= lastSizeJobIndex * 0.8)
                {
                    int repeated = 0;
                    while (STInstance->exists(state, job) == 1 && repeated++ < 1)
                    {
                        logging(state, job, "suspend");
                        if (makespan > upperBound || foundOptimal || cancel)
                        {
                            logging(state, job, "after not worth continuing");
                            return;
                        }
                        tbb::task::suspend([&](oneapi::tbb::task::suspend_point tag)
                            {
                                STInstance->addDelayed(state, job,  tag);
                                return; 
                            }
                        );
                        logging(state, job, "restarted");
                        if (makespan > upperBound || foundOptimal || cancel)
                        {
                            logging(state, job, "after not worth continuing");
                            return;
                        }
                    }

                    if (STInstance->exists(state, job) == 2)
                    {
                        logging(state, job, "after restard found");
                        return;
                    }
                }
                break;
            default:
                logging(state, job, "gist not found");
                STInstance->addPreviously(state, job);
            }
        }

        // FUR
        if (fur)
        {
            bool retFlag;
            executeFUR(state, job, retFlag);
            if (retFlag)
                return;
        }
        logging(state, job, "after Fur");

        tbb::task_group tg;
        int endState = numMachines;
        // index of assignments that are currently ignored by Rule 6 (same Ret entry than the previous job and can therefore be ignored)
        std::vector<int> repeat;

        // index of assignments, that would lead to a gist that is already handled somewhere else
        std::vector<int> delayed;
        // Rule 4 
        if (numMachines > lastRelevantJobIndex - job + 1)
            endState = lastRelevantJobIndex - job + 1;

        assert(endState <= numMachines);
        // base case
        executeBaseCase(endState, state, job, repeat, tg, delayed);

        logging(state, job, "wait for tasks to finish");
        tg.wait();
        logging(state, job, "execute Delayed Tasks");
        executeDelayed(delayed, state, job, tg);
        logging(state, job, "after Delayed Tasks");
        executeR6Delayed(repeat, state, job, tg);
        tg.wait();

        if (gist)
        {

            STInstance->addGist(state, job);
        }
        logging(state, job, "after add");
        return;
    }
    inline void executeR6Delayed(const std::vector<tbb::detail::d1::numa_node_id> &repeat, const std::vector<tbb::detail::d1::numa_node_id> &state, const int job, tbb::detail::d1::task_group &tg)
    {
        for (int i : repeat)
        {
            if (!lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                auto next = new std::vector<int>(state);
                (*next)[i] += jobDurations[job];
                resortAfterIncrement(*next, i);
                logging(*next, job + 1, "Rule 6 from recursion");
                tg.run([this, next, job]
                       { solvePartial(*next, job + 1);
                       delete next; });
            }
        }
    }
    inline void executeDelayed(std::vector<tbb::detail::d1::numa_node_id> &delayed, const std::vector<tbb::detail::d1::numa_node_id> &state, const int job, tbb::detail::d1::task_group &tg)
    {
        int count = 0;
        std::vector<tbb::detail::d1::numa_node_id> again;
        again.reserve(delayed.size());
        // allPrev += delayed.size();
        for (int i = 0; i < 1; i++)
        {
            for (int i : delayed)
            {
                auto next = new std::vector<int>(state);
                (*next)[i] += jobDurations[job];
                resortAfterIncrement(*next, i);
                auto ex = STInstance->exists(*next, job + 1);
                if (ex == 0)
                {

                    tg.run([this, next, job]
                           { solvePartial(*next, job + 1); delete next; });
                    if (++count >= limitedTaskSpawning)
                    {
                        tg.wait();
                        count = 0;
                    }
                }
                else if (ex == 1)
                { // of the former delayed tasks try a better order of solving
                    again.push_back(i);
                    delete next;
                } else {
                    delete next;
                }
            }
            if (again.size() == delayed.size())
                break;
            delayed.clear();
            for (int i : again)
            {
                auto next = new std::vector<int>(state);
                (*next)[i] += jobDurations[job];
                // std::sort(next.begin(), next.end());
                resortAfterIncrement(*next, i);

                auto ex = STInstance->exists(*next, job + 1);
                if (ex == 0)
                {
                    logging(*next, job + 1, "child from recursion");
                    tg.run([this, next, job]
                           { solvePartial(*next, job + 1); delete next; });
                    if (++count >= limitedTaskSpawning)
                    {
                        tg.wait();
                        count = 0;
                    }
                }
                else if (ex == 1)
                { 
                    delayed.push_back(i);
                    delete next;    
                } else {
                    delete next;
                }
            }
            again.clear();
        }
        tg.wait();
        for (int i : delayed)
        {
            auto next = new std::vector<int>(state);
            (*next)[i] += jobDurations[job];
            resortAfterIncrement(*next, i);
            // runWithPrev++;
            logging(*next, job + 1, "child from recursion");
            tg.run([this, next, job]
                   { solvePartial(*next, job + 1); delete next; });
            if (++count >= limitedTaskSpawning)
            {
                tg.wait();
                count = 0;
            }
        }
        tg.wait();
    }
    inline void executeBaseCase(const int endState, const std::vector<tbb::detail::d1::numa_node_id> &state, const int job, std::vector<tbb::detail::d1::numa_node_id> &repeat, tbb::detail::d1::task_group &tg, std::vector<tbb::detail::d1::numa_node_id> &delayed)
    {
        int count = 0;
        for (int i = 0; i < endState; i++)
        {
            if ((i > 0 && state[i] == state[i - 1]) || state[i] + jobDurations[job] > upperBound)
                continue; // Rule 1 + check directly wether the new state would be feasible
            if (i > 0 && lookupRet(state[i], state[i - 1], job))
            { // Rule 6
                repeat.push_back(i);
                continue;
            }
            auto next = new std::vector<int>(state);
            (*next)[i] += jobDurations[job];
            resortAfterIncrement(*next, i);

            // filter delayed / solved assignments directly before spawning the tasks
            if (gist)
            {
                auto ex = STInstance->exists(*next, job + 1);
                switch (ex)
                {
                case 0:
                    if (job > lastSizeJobIndex / 4)
                    {
                        logging(*next, job + 1, "child from recursion");
                        tg.run([this, next, job]
                               { solvePartial(*next, job + 1); delete next; });
                        if (++count >= limitedTaskSpawning)
                        {
                            tg.wait();
                            count = 0;
                        }
                    }
                    else
                    {
                        logging(*next, job + 1, "child from recursion");
                        tg.run([this, next, job]
                               { solvePartial(*next, job + 1); delete next; });
                    }

                    break;
                case 1:
                    delete next;
                    delayed.push_back(i);
                    break;
                default:
                    delete next;
                    continue;
                    break;
                }
            }
            else
            {
                if (job > lastSizeJobIndex / 4)
                {

                    tg.run([this, next, job]
                           { solvePartial(*next, job + 1); delete next; });
                    if (++count >= limitedTaskSpawning)
                    {
                        tg.wait();
                        count = 0;
                    }
                }
                else
                {
                    tg.run([this, next, job]
                           { solvePartial(*next, job + 1); delete next; });
                }
            }
        }
    }
    inline void executeFUR(const std::vector<tbb::detail::d1::numa_node_id> &state, int job, bool &retFlag)
    {
        retFlag = true;
        for (int i = (numMachines - 1); i >= 0; i--)
        {
            if (state[i] + jobDurations[job] <= upperBound && lookupRetFur(state[i], jobDurations[job], job))
            {
                auto next = new std::vector<int>(state);
                assert(*next == state);
                // might not need to copy here but carefully
                (*next)[i] += jobDurations[job];
                resortAfterIncrement(*next, i);

                solvePartial(*next, job + 1);
                delete next;
                if (state[i] + jobDurations[job] <= upperBound && lookupRetFur(state[i], jobDurations[job], job))
                {
                    if (gist)
                    {
                        STInstance->addGist(state, job);
                    }
                    logging(state, job, "after add");
                    return;
                }
            }
        }
        retFlag = false;
    }
    template <typename T>
    void logging(const std::vector<int> &state, int job, T message = "")
    {
        if (!detailedLogging)
            return;

        std::stringstream gis;
        gis << message << " ";
        for (auto vla : state)
            gis << vla << ", ";
        gis << " => ";
        for (auto vla : STInstance->computeGist(state, job))
            gis << vla << ", ";
        gis << " Job: " << job;
        std::cout << gis.str() << std::endl;
    }

    inline bool lookupRet(int i, int j, int job)
    {
        const int off = offset.load();
        if (std::max(i, j) + off >= (int)RET[job].size())
            return false;
        assert(i + off < (int)RET[job].size() && j + off < (int)RET[job].size());
        return RET[job][i + off] == RET[job][j + off];
    }
    inline bool lookupRetFur(int i, int j, int job)
    {
        const int off = offset.load();
        if (i + off >= (int)RET[job].size())
            return false;
        assert(i + off < (int)RET[job].size() && initialUpperBound - j < (int)RET[job].size());
        return RET[job][i + off] == RET[job][initialUpperBound - j];
    }

    /**
     * @brief runs Lpt on the given partialassignment
     */
    void lpt(std::vector<int> &state, int job)
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
            STInstance->resumeAllDelayedTasks(); // TODO work with a finished flag that should be more performant
        }

        if (logBound)
            std::cout << "try Bound " << newBound << std::endl;

        if (newBound > upperBound)
            return;
        if (logBound)
            std::cout << "new Bound " << newBound << std::endl;
        std::unique_lock lock(boundLock, std::try_to_lock); // this one does not appear often so no need to optimize that
        while (!lock.owns_lock())
        {
            std::this_thread::yield();
            if (newBound > upperBound.load())
                return; // TODO check wether this works fine because we end an exploration path even though the bound is not yet updated
            lock.try_lock();
        }
        if (newBound > upperBound)
            return;
        const int newOffset = initialUpperBound - (newBound - 1);
        if (gist) // it is importatnt to d othe bound update on the ST before updating the offset/upperBound, because otherwise the exist might return a false positive (i am not 100% sure why)
        {
            STInstance->prepareBoundUpdate();
        }
        upperBound.store(newBound - 1);
        offset.store(newOffset);
        if (gist) // it is importatnt to d othe bound update on the ST before updating the offset/upperBound, because otherwise the exist might return a false positive (i am not 100% sure why)
        {
            STInstance->boundUpdate(newOffset);
        }
        upperBound.store(newBound - 1);
        offset.store(newOffset);

        auto newTime = std::chrono::high_resolution_clock::now();
        timeFrames.push_back((std::chrono::duration<double>)(newTime - lastUpdate));
        lastUpdate = newTime;
        if (logBound)
            std::cout << "new Bound " << newBound << "finished" << std::endl;
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
     * @brief find index, to when jobs are no longer relevant regarding the given Bound.
     */
    int computeIrrelevanceIndex(int bound)
    {
        std::vector<int> sum(jobDurations.size() + 1);
        sum[0] = 0;

        std::partial_sum(jobDurations.begin(), jobDurations.end(), sum.begin() + 1);
        int i = jobDurations.size() - 1;
        while (i > 0 && sum[i] < numMachines * (lowerBound - jobDurations[i] + 1))
        { 
            i--;
        }

        return i;
    }
    double getMemoryUsagePercentage()
    {
        long totalPages = sysconf(_SC_PHYS_PAGES);
        long availablePages = sysconf(_SC_AVPHYS_PAGES);
        long pageSize = sysconf(_SC_PAGE_SIZE);

        if (totalPages > 0)
        {
            long usedPages = totalPages - availablePages;
            double usedMemory = static_cast<double>(usedPages * pageSize);
            double totalMemory = static_cast<double>(totalPages * pageSize);
            return (usedMemory / totalMemory) * 100.0;
        }
        return -1.0; // Fehlerfall
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
        timeFrames.clear();
        if (STInstance != nullptr)
        {
            STInstance->clear();
            delete STInstance;
        }
        STInstance = nullptr;
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

    void initializeST()
    {
        switch (STtype)
        {
            // STImpl currently not maintained
        // case 0:
        //     STInstance = new STImpl(lastRelevantJobIndex + 1, offset, RET, numMachines);
        //     break;
        case 2:
            STInstance = new STImplSimplCustomLock(lastRelevantJobIndex + 1, offset, RET, numMachines, 2);
            break;
        default:
            STInstance = new STImplSimplCustomLock(lastRelevantJobIndex + 1, offset, RET, numMachines, 0);
        }
    }
};

#endif