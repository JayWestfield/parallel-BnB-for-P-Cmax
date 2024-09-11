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
    using HashMap = tbb::concurrent_hash_map<std::vector<int>,bool , VectorHasher>;

    STImplSimpl(int jobSize, int offset, std::vector<std::vector<int>> *RET, std::size_t vec_size) : ST(jobSize, offset, RET, vec_size), maps(jobSize) {
        initializeThreadLocalVector(vec_size + 1);
    }
    std::vector<int> computeGist(const std::vector<int> &state, int job) override
    {
        // assume the state is sorted
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));
        if ((state.back() + offset) >= maximumRETIndex)
            throw std::runtime_error("infeasible");
        std::vector<int> gist(vec_size + 1, 0);
        assert((state.back() + offset) < maximumRETIndex); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < vec_size; i++)
        {
            gist[i] = (*RET)[job][state[i] + offset];
        }
        gist[vec_size] = job;
        return gist;
    }
    void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) override
    {
        initializeThreadLocalVector(vec_size + 1);
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));
        if ((state.back() + offset) >= maximumRETIndex)
            throw std::runtime_error("infeasible");
        assert((state.back() + offset) < (*RET)[job].size()); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < vec_size; i++)
        {
            gist[i] = (*RET)[job][state[i] + offset];
        }
        gist[vec_size] = job;
    }

    void addGist(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize && job >= 0);
        std::shared_lock<std::shared_mutex> lock(updateBound);
        computeGist2(state, job, threadLocalVector);
        HashMap::accessor acc;
        maps.insert(acc, threadLocalVector);
        acc->second = true;
        assert(acc->second == true);
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);

        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize)
        {
            computeGist2(state, job, threadLocalVector);
            HashMap::const_accessor acc;
            if (maps.find(acc, threadLocalVector))
            {
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }

    void addPreviously(const std::vector<int> &state, int job) override
    {
        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize)
        {
            computeGist2(state, job, threadLocalVector);

            HashMap::accessor acc;
            if (maps.find(acc, threadLocalVector)) 
                return;
            if (maps.insert(acc, threadLocalVector))
                acc->second = false;
        }
        logging(state, job, "add Prev");
    }

    void boundUpdate(int offset) override
    {

        std::unique_lock<std::shared_mutex> lock(updateBound);
        this->offset = offset;
        clear();
    }
    void clear() override
    {
        HashMap newMaps(jobSize);
        maps.swap(newMaps);
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
    std::shared_mutex updateBound; // shared_mutex zum Schutz der Hashmaps während boundUpdate
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
