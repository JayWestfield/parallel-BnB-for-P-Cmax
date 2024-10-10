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
struct VectorHasher
    {
        inline void hash_combine(std::size_t &s, const tbb::detail::d1::numa_node_id &v) const
        {
            s ^= hashing::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
        }
        size_t hash(const std::vector<tbb::detail::d1::numa_node_id> &vec)
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        size_t operator()(const std::vector<tbb::detail::d1::numa_node_id> &vec) const
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        bool equal(const std::vector<int> &a, const std::vector<int> &b) const
        {
            return std::equal(a.begin(), a.end(), b.begin());
        }
    };
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
        if (maps != nullptr)
            delete maps;
    }

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
        // if ((state[vec_size - 1]+ offset) >= maximumRETIndex)
        //     throw std::runtime_error("infeasible");
        assert((state.back() + offset) < maximumRETIndex); // TODO maybe need error Handling to check that
        for (auto i = 0; i < vec_size; i++)
        {
            gist[i] = RET[job][state[i] + offset];
        }
        gist[vec_size] = job;
    }

    void addGist(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize && job >= 0);
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
        // resumeGist(threadLocalVector);
        maps->insert(threadLocalVector, true);
        std::stringstream gis;
        gis << "added gist" << " ";
        for (auto vla : threadLocalVector)
            gis << vla << ", ";
        gis << std::endl;
        std::cout << gis.str() << std::flush;
        resumeGist(threadLocalVector);
        // resumeAllDelayedTasks();
        
        referenceCounter--;
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);
        // if (offset != 40) return 0;
        // problem clearflag und referenceCounter zwischendrin kann ein clear kommen !!!
        if (clearFlag || skipThis(job))
            return 0;
        assert(job >= 0 && job < jobSize);
        if ((state[vec_size - 1] + offset) >= maximumRETIndex)
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
    // TODO addprev must return a boolena for the out of bounds check
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
            // std::cout << referenceCounter.load() << std::endl;
        }
        maps->clear();
        this->offset = offset;
        resumeAllDelayedTasks();
        clearFlag = false;
    }
    void prepareBoundUpdate()
    {
        clearFlag = true;
    }
    void clear() override
    {

        maps->clear();
    }

    void resumeAllDelayedTasks() override
    {
        delayedLock.lock();
        delayedMap.resumeAll();
        // for (std::tuple<std::vector<int>, tbb::task::suspend_point> entry : delayedMap) {
        //     tbb::task::resume(std::get<1>(entry));
        // }
        // delayedMap.clear();
        delayedLock.unlock();

    }

    void addDelayed(const std::vector<int> &state, int job, tbb::task::suspend_point tag) override
    {
        assert(job < jobSize && job >= 0);

        // std::cout << "add delay "  << std::endl;
        delayedLock.lock();
        if (clearFlag)
        {
            delayedLock.unlock();
            tbb::task::resume(tag);
            return;
        }
        
        computeGist2(state, job, threadLocalVector);
        if (maps->find(threadLocalVector) != 1){
            tbb::task::resume(tag);
        } else {
            delayedMap.insert(threadLocalVector, tag);
            // delayedMap.push_back(std::make_tuple(threadLocalVector, tag));
        }
        if (maps->find(threadLocalVector) != 1){
            delayedMap.resume(threadLocalVector);
        }
        std::ostringstream oss;
        oss << "check for the gists of delayed Tasks current gist ";
        for (auto vla : threadLocalVector)
                oss << vla << ", ";
        oss <<  std::endl;
        for (auto entry : delayedMap.getNonEmptyKeys()) {
            for (auto vla : entry)
                oss << vla << ", ";
            oss << "exists coorekt: " <<  maps->find(entry) << std::endl;
        }
        std::cout << oss.str() << std::flush;
        delayedLock.unlock();
        logging(state, job, "endDelay");
    }

private:
    bool detailedLogging = false;
    IConcurrentHashMap *maps = nullptr;
    bool useBitmaps = false;
    // std::vector<std::tuple<std::vector<int>, tbb::task::suspend_point>>  delayedMap;
    // std::unordered_map<std::vector<int>, std::vector<tbb::task::suspend_point>, VectorHasher> delayedMap;
    HashMapWrapper delayedMap;
    std::mutex delayedLock;

    void resumeGist(std::vector<int> gist) {
        delayedLock.lock();
        delayedMap.resume(gist);
        // auto const resume = delayedMap.find(gist);
        // if (resume != delayedMap.end()) {
        //     for (auto task : (std::get<1>(*resume))) {
        //         tbb::task::resume(task);
        //     }
        // }
        
        // for (auto entry : delayedMap) {
        //     if (std::get<0>(entry) == gist) {
        //         tbb::task::resume(std::get<1>(entry));
        //     }
        // }
        std::ostringstream oss;
        oss << "check for the gists of delayed Tasks current gist ";
        for (auto vla : gist)
                oss << vla << ", ";
        oss <<  std::endl;
        for (auto entry : delayedMap.getNonEmptyKeys()) {
            for (auto vla : entry)
                oss << vla << ", ";
            oss << "exists: " <<  maps->find(entry) << std::endl;
        }
        std::cout << oss.str() << std::flush;
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
    // custom lock
    std::atomic<u_int32_t> referenceCounter;
    bool clearFlag;
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

    void evictAll()
    {
        clearFlag = true;
        while (referenceCounter.load() > 0)
        {
            // wait maybe yield etc
        }
        maps->clear();
        this->offset = offset;
        clearFlag = false;
    }

};

#endif
