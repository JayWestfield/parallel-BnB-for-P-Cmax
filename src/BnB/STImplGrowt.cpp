#ifndef ST_IMPL_Growt_H
#define ST_IMPL_Growt_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <random>
#include "data-structures/table_config.hpp"
#include "allocator/alignedallocator.hpp"
const int startSize = 200;
class STImplGrowt : public ST
{

public:
struct boolWriteTrue
{
    using mapped_type = bool;

    mapped_type operator()(mapped_type& lhs, const mapped_type& rhs) const
    {
        lhs = rhs;
        return rhs;
    }

    // an atomic implementation can improve the performance of updates in .sGrow
    // this will be detected automatically
    mapped_type atomic(mapped_type& lhs, const mapped_type& rhs) const
    {

        lhs = rhs;
        return rhs;
    }

    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
};
    using allocator_type = growt::AlignedAllocator<bool>;

    using HashMap = typename growt::table_config<std::vector<tbb::detail::d1::numa_node_id>, bool, VectorHasher, allocator_type,
                                                 hmod::growable, hmod::deletion>::table_type;
    // using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;

    STImplGrowt(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : ST(jobSize, offset, RET, vec_size), maps(startSize)
    {
        initializeThreadLocalVector(vec_size + 1);
        referenceCounter = 0;
        clearFlag = false;
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
        if (clearFlag)
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
        auto handle = maps.get_handle();
        handle.insert_or_update(threadLocalVector, true, boolWriteTrue(), true);
        // HashMap::accessor acc;
        // maps.insert(acc, threadLocalVector);
        // acc->second = true;
        // assert(acc->second == true);
        referenceCounter--;
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);
        // if (offset != 40) return 0;
        // problem clearflag und referenceCounter zwischendrin kann ein clear kommen !!!
        if (clearFlag)
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
        auto handle = maps.get_handle();
        auto temp = handle.find(threadLocalVector);
        int result = 0;
        if (temp != handle.end())
        {
            result =  (*temp).second ? 2 : 1;
        };
        referenceCounter--;
        return result;
        // HashMap::const_accessor acc;
        // int result = 0;
        // if (maps.find(acc, threadLocalVector))
        // {
        //     result = acc->second ? 2 : 1;
        // }
        // return result;
    }
    // TODO addprev must return a boolena for the out of bounds check
    void addPreviously(const std::vector<int> &state, int job) override
    {
        assert(job >= 0 && job < jobSize);
        if (clearFlag)
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
        auto handle = maps.get_handle();
        handle.insert(threadLocalVector, false);
        // HashMap::accessor acc;
        // if (maps.insert(acc, threadLocalVector))
        //     acc->second = false;
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
                std::cout << "probably stuck in an endless loop"  << referenceCounter << std::endl;
            std::this_thread::yield();
            // std::cout << referenceCounter.load() << std::endl;
        }
        maps = HashMap(startSize);
        // maps.clear();
        this->offset = offset;
        clearFlag = false;
    }
    void prepareBoundUpdate()
    {
        clearFlag = true;
    }
    void clear() override
    {
        maps = HashMap(startSize);
        // HashMap newMaps;
        // newMaps.clear();
        // maps.swap(newMaps);
        // maps.clear();
    }

    void resumeAllDelayedTasks() override
    {
    }

    void addDelayed(const std::vector<int> &gist, int job, tbb::task::suspend_point tag) override
    {
    }

private:
    bool detailedLogging = false;
    HashMap maps;
    bool useBitmaps = false;

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
        maps = HashMap(startSize);

        // maps.clear();
        this->offset = offset;
        clearFlag = false;
    }
};

#endif
