#ifndef ST_IMPL_Simpl_H
#define ST_IMPL_Simpl_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <random>
class STImplSimpl : public ST
{

public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;

    STImplSimpl(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : ST(jobSize, offset, RET, vec_size)
    {
        initializeThreadLocalVector(vec_size + 1);
    }
    ~STImplSimpl(){
                std::unique_lock<std::shared_mutex> lock(clearLock);
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
        std::shared_lock<std::shared_mutex> lock(clearLock);
        if ((state[vec_size - 1]+ offset) >= maximumRETIndex) return;
        computeGist2(state, job, threadLocalVector);
        HashMap::accessor acc;
        maps.insert(acc, threadLocalVector);
        acc->second = true;
        assert(acc->second == true);
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);
        // if (offset != 40) return 0;
        std::shared_lock<std::shared_mutex> lock(clearLock, std::try_to_lock);
        if (!lock.owns_lock()) return 0;
        if (job >= 0 && job < jobSize)
        {
            if ((state[vec_size - 1]+ offset) >= maximumRETIndex) return 0;
            computeGist2(state, job, threadLocalVector);
            HashMap::const_accessor acc;
            if (maps.find(acc, threadLocalVector))
            {
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }
    // TODO addprev must return a boolena for the out of bounds check
    void addPreviously(const std::vector<int> &state, int job) override
    {
        assert(job >= 0 && job < jobSize);
        std::shared_lock<std::shared_mutex> lock(clearLock, std::try_to_lock);
        if (!lock.owns_lock()) return;
        if ((state[vec_size - 1]+ offset) >= maximumRETIndex) return;

        computeGist2(state, job, threadLocalVector);

        HashMap::accessor acc;
        if (maps.insert(acc, threadLocalVector))
            acc->second = false;
        logging(state, job, "add Prev");
    }

    void boundUpdate(int offset) override
    {

        std::unique_lock<std::shared_mutex> lock(clearLock);
        if (offset <= this->offset) return;
        clear();
        this->offset = offset;
    }
    void prepareBoundUpdate() override
    {
    }

    void clear() override
    {
        HashMap newMaps;
        newMaps.clear();
        maps.swap(newMaps);
        maps.clear();
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
    std::shared_mutex mapsLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    bool useBitmaps = false;
    std::shared_mutex clearLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate
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
        std::unique_lock<std::shared_mutex> lock(mapsLock); // todo smarter clean?
        HashMap newMap;
        maps.swap(newMap);
    }
};

#endif
