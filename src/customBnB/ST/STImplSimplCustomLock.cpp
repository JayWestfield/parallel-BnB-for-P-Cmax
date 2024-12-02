#ifndef ST_IMPL_Simpl_Custom_lock_H
#define ST_IMPL_Simpl_Custom_lock_H

#include "./ST_custom.h"
#include <memory>
#include <ostream>
#include <tbb/tbb.h>
#include <iostream>
#include <sstream>
#include <cassert>
#include <unistd.h>
#include "hashmap/IConcurrentHashMap.h"
#include "hashmap/TBBHashMap.cpp"
#include "hashmap/delayedHashMaps/stdDelayedMap.cpp"
// #include "./hashmap/FollyHashMap.cpp"
// #include "./hashmap/JunctionHashMap.cpp"
// #include "./hashmap/GrowtHashMap.cpp"
#include "customSharedLock.hpp"
#include "../structures/SuspendedTaskHolder.hpp"
class STImplSimplCustomLock : public ST_custom
{
public:
    // using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;

    STImplSimplCustomLock(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size, ITaskHolder &suspendedTasks, int HashmapType) : ST_custom(jobSize, offset, RET, vec_size), suspendedTasks(suspendedTasks), delayedMap(suspendedTasks)
    {
        initializeHashMap(HashmapType);
        // referenceCounter = 0;
        // clearFlag = false;
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
        if ((state.back() + offset) >= maximumRETIndex ) return {};
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
        if ((state[vec_size - 1] + offset) >= maximumRETIndex ||  skipThis(job))
            return;

        // referenceCounter++;
        // if (clearFlag)
        // {
        //     referenceCounter--;
        //     return;
        // }
        CustomTrySharedLock lock(mtx);
        if (!lock.owns_lock()) return;
        if ((state[vec_size - 1] + offset) >= maximumRETIndex ||  skipThis(job))
            return;
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

        if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
            return 0;
        
        CustomTrySharedLock lock(mtx);
        if (!lock.owns_lock()) return 0;
        if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
            return 0;
        // referenceCounter++;
        // if (clearFlag)
        // {
        //     referenceCounter--;
        // }
        computeGist2(state, job, threadLocalVector);
        const int result = maps->find(threadLocalVector);
        referenceCounter--;
        return result;
    }

    void addPreviously(const std::vector<int> &state, int job) override
    {
        assert(job >= 0 && job < jobSize);
        if (skipThis(job))
            return;
        if ((state[vec_size - 1] + offset) >= maximumRETIndex)
            return;
        CustomTrySharedLock lock(mtx);
        if (!lock.owns_lock()) return;
        if ((state[vec_size - 1] + offset) >= maximumRETIndex)
            return;
        // referenceCounter++;
        // if (clearFlag)
        // {
        //     referenceCounter--;
        //     return;
        // }
        computeGist2(state, job, threadLocalVector);
        maps->insert(threadLocalVector, false);
        logging(state, job, "add Prev");
        // referenceCounter--;
    }

    // assert bound update is itself never called in parallel
    void boundUpdate(int offset) override
    {
        if (offset <= this->offset)
            return;
        CustomUniqueLock lock(mtx);
        // clearFlag = true;
        // int test = 0;
        // while (referenceCounter.load() > 0)
        // {
        //     if (test++ > 1000000)
        //         std::cout << "probably stuck in an endless loop" << std::endl;
        //     std::this_thread::yield();
        // }
        maps->clear();
        this->offset = offset;
        resumeAllDelayedTasks();
        // clearFlag = false;
    }
    void prepareBoundUpdate() override
    {
        // clearFlag = true;
        mtx.clearFlag = true;
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

    void addDelayed(std::shared_ptr<CustomTask> task) override
    {
        auto &state = task->state;
        auto job = task->job;
        assert(job < jobSize && job >= 0);
        delayedLock.lock();
        CustomTrySharedLock lock(mtx);
        if (clearFlag || !lock.owns_lock())
        {
            suspendedTasks.addTask(std::move(task));
            delayedLock.unlock();
            return;
        }
        if ((state[vec_size - 1] + offset) >= maximumRETIndex )
        {
            // TODO instead of restarting just unregister this child from parent
            suspendedTasks.addTask(std::move(task));
        }
        
        computeGist2(state, job, threadLocalVector);
        if (maps->find(threadLocalVector) != 1){
            suspendedTasks.addTask(std::move(task));
        } else {
            delayedMap.insert(threadLocalVector, task);
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
    ITaskHolder &suspendedTasks;
    // custom shared lock
    std::atomic<u_int32_t> referenceCounter;
    bool clearFlag;
    CustomSharedMutex mtx;
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
    void cancelExecution() override {
        // clearFlag = true;
        // while (referenceCounter.load() > 0)
        // {
        //     std::this_thread::yield();
        // }
        CustomUniqueLock lock(mtx);
        delayedLock.lock();
        delayedMap.cancelExecution();
        delayedLock.unlock();
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
