#ifndef ST_IMPL_Simpl_Custom_lock_H
#define ST_IMPL_Simpl_Custom_lock_H

#include "ST.h"
#include <ostream>
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <random>
#include "./hashmap/IConcurrentHashMap.h"
#include "./hashmap/TBBHashMap.cpp"
#include "./hashmap/delayedHashMaps/stdDelayedMap.cpp"
// #include "./hashmap/FollyHashMap.cpp"
// #include "./hashmap/JunctionHashMap.cpp"
// #include "./hashmap/GrowtHashMap.cpp"

class STImplSimplCustomLock : public ST
{
public:
    // using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;

    STImplSimplCustomLock(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size, int HashmapType) : ST(jobSize, offset, RET, vec_size)
    {
        initializeThreadLocalVector(vec_size + 1);
        initializeHashMap(HashmapType);
        referenceCounter = 0;
        clearFlag = false;
    }

    ~STImplSimplCustomLock()
    {
        resumeAllDelayedTasks();
        if (maps != nullptr)
            delete maps;
    }

    // Deprecated use ComputeGist2 instead
    std::vector<int> computeGist(const std::vector<int> &state, int job) override
    {
        // assume the state is sorted
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));
        std::vector<int> gist(vec_size + 1, 0);
        assert((state.back() + offset) < maximumRETIndex); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < vec_size; i++)
        {
            gist[i] = RET[job][state[i] + offset];
        }
        gist[vec_size] = job;
        return gist;
    }
    void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) override
    {
        initializeThreadLocalVector(vec_size + 1);
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));
        assert((state.back() + offset) < maximumRETIndex);
        for (auto i = 0; i < vec_size; i++)
        {
            gist[i] = RET[job][state[i] + offset];
        }
        gist[vec_size] = job; // TODO think about wether we really need the jobdepth here (can 2 partial assignments with a different depth have teh same gist and if so is that a problem?)
    }

    void addGist(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize && job >= 0);
        if (clearFlag || (state[vec_size - 1] + offset) >= maximumRETIndex ||  skipThis(job))
            return;

        referenceCounter++;
        if (clearFlag)
        {
            referenceCounter--;
            return;
        }
        computeGist2(state, job, threadLocalVector);
        maps->insert(threadLocalVector, true);
        logging(state,  job, "added Gist");
        resumeGist(threadLocalVector);      
        referenceCounter--;
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);
        assert(job >= 0 && job < jobSize);

        if (clearFlag || (state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
            return 0;

        referenceCounter++;
        if (clearFlag)
        {
            referenceCounter--;
            return 0;
        }
        computeGist2(state, job, threadLocalVector);
        const int result = maps->find(threadLocalVector);
        referenceCounter--;
        return result;
    }

    void addPreviously(const std::vector<int> &state, int job) override
    {
        assert(job >= 0 && job < jobSize);
        if (clearFlag || skipThis(job))
            return;
        if ((state[vec_size - 1] + offset) >= maximumRETIndex)
            return;

        referenceCounter++;
        if (clearFlag)
        {
            referenceCounter--;
            return;
        }
        computeGist2(state, job, threadLocalVector);
        maps->insert(threadLocalVector, false);
        logging(state, job, "add Prev");
        referenceCounter--;
    }

    // assert bound update is itself never called in parallel
    void boundUpdate(int offset) override
    {
        if (offset <= this->offset)
            return;
        clearFlag = true;
        int test = 0;
        while (referenceCounter.load() > 0)
        {
            if (test++ > 1000000)
                std::cout << "probably stuck in an endless loop" << std::endl;
            std::this_thread::yield();
        }
        maps->clear();
        this->offset = offset;
        resumeAllDelayedTasks();
        clearFlag = false;
    }
    void prepareBoundUpdate() override
    {
        clearFlag = true;
    }

    void clear() override
    {
        resumeAllDelayedTasks();
        maps->clear();
    }

    void resumeAllDelayedTasks() override
    {
        delayedLock.lock();
        delayedMap.resumeAll();
        delayedLock.unlock();
    }

    void addDelayed(const std::vector<int> &state, int job, tbb::task::suspend_point tag) override
    {
        assert(job < jobSize && job >= 0);
        delayedLock.lock();
        if (clearFlag)
        {
            tbb::task::resume(tag);
            delayedLock.unlock();
            return;
        }
        
        computeGist2(state, job, threadLocalVector);
        if (maps->find(threadLocalVector) != 1){
            tbb::task::resume(tag);
        } else {
            delayedMap.insert(threadLocalVector, tag);
        }
        // this should not be needed but just to be safe
        if (maps->find(threadLocalVector) != 1){
            delayedMap.resume(threadLocalVector);
        }
        logSuspendedTasks(threadLocalVector);
        logging(state, job, "endDelay");
        delayedLock.unlock();
    }

private:
    bool detailedLogging = false;
    IConcurrentHashMap *maps = nullptr;
    bool useBitmaps = false; // currently not supported
    HashMapWrapper delayedMap;
    std::mutex delayedLock;

    // custom shared lock
    std::atomic<u_int32_t> referenceCounter;
    bool clearFlag;

    void logSuspendedTasks(const std::vector<int>& gist) {
        if (!detailedLogging) {
            return;
        }
        std::ostringstream oss;
        oss << "check gists of currently suspended Tasks:";
        for (auto vla : gist)
                oss << vla << ", ";
        oss <<  std::endl;
        for (auto entry : delayedMap.getNonEmptyKeys()) {
            for (auto vla : entry)
                oss << vla << ", ";
            oss << "exists: " <<  maps->find(entry) << std::endl;
        }
        std::cout << oss.str() << std::flush;
    }
    void resumeGist(const std::vector<int>& gist) {
        delayedLock.lock();
        delayedMap.resume(gist);
        logSuspendedTasks(gist);
        delayedLock.unlock();
    }
    
    void initializeHashMap(int type)
    {
        if (maps != nullptr)
            delete maps;
        switch (type)
        {
        case 0:
            maps = new TBBHashMap();
            break;
        // case 1:
        //     maps = new FollyHashMap();
        //     break;
        // case 2:
            // maps = new GrowtHashMap();
            // break;
        // case 3 :
        //     maps = new JunctionHashMap();
        //     break;
        default:
            maps = new TBBHashMap();
        }
    }

    // experimental to not add every depth to the ST
    inline bool skipThis(int depth)
    {
        return false; // depth % 2 == 0;
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
        for (auto vla : computeGist(state, job))
            gis << vla << " ";
        gis << " Job: " << job << "\n";
        std::cout << gis.str() << std::endl;
    }
};

#endif
