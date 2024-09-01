#ifndef ST_IMPL_H
#define ST_IMPL_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>
#include <sstream>
#include <cassert>
#include <stdexcept>
#include <unistd.h>
#include <random>

struct Key {
    std::vector<int> vec;
    int number;
    Key(const std::vector<int>& v, int num) : vec(v), number(num) {}
    bool operator==(const Key& other) const {
        return vec == other.vec && number == other.number;
    }
};
struct VectorHasherSimpl {
    std::size_t operator()(const Key& k) const {
        std::size_t seed = k.vec.size();
        for (auto& i : k.vec) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        seed ^= k.number + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }

    bool equal(const Key& k1, const Key& k2) const {
        return k1 == k2;
    }
};
class STImpl : public ST
{
public:
    using HashMap = tbb::concurrent_hash_map<Key, bool, VectorHasherSimpl>;

    STImpl(int jobSize, int offset, std::vector<std::vector<int>> *RET) : ST(jobSize, offset, RET), maps(jobSize) {}
    // assume the state is sorted
    std::vector<int> computeGist(std::vector<int> state, int job) override
    {
        assert(job < jobSize && job >= 0);
        std::sort(state.begin(), state.end());
        if ((long unsigned int)(state.back() + offset) >= (*RET)[job].size())
            throw std::runtime_error("infeasible");
        std::vector<int> gist(state.size(), 0);
        assert((long unsigned int)(state.back() + offset) < (*RET)[job].size()); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < state.size(); i++)
        {
            gist[i] = (*RET)[job][state[i] + offset];
        }
        return gist;
    }

    void addGist(std::vector<int> state, int job) override
    {
        assert(job < jobSize && job >= 0);
        if (getMemoryUsagePercentage() > 80)
        {
            std::random_device rd;                                   // obtain a random number from hardware
            std::mt19937 gen(rd());                                  // seed the generator
            std::uniform_int_distribution<> distr(0, this->jobSize); // define the range
            const int other = distr(gen);
            std::cout << "skipped " << maps.size() << " " << maps.size() << std::endl;
                    double memoryUsage = getMemoryUsagePercentage();
            std::cout << "Memory usage high: " << memoryUsage << "%. Calling evictAll." << std::endl;
            evictAll();
            
            // Überprüfe die Speicherauslastung nach evictAll
            memoryUsage = getMemoryUsagePercentage();
            std::cout << "Memory usage after evictAll: " << memoryUsage << "%" << std::endl;
       
            return;
        }

        std::shared_lock<std::shared_mutex> lock(updateBound);

        HashMap::accessor acc;
        maps.insert(acc, Key(computeGist(state, job), job));
        acc->second = true;
        assert(acc->second == true);
    }

    int exists(std::vector<int> state, int job) override
    {
        assert(job < jobSize);

        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize)
        {
            HashMap::const_accessor acc;
            if (maps.find(acc, Key(computeGist(state, job), job)))
            {
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }

    void addPreviously(std::vector<int> state, int job) override
    {
        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize)
        {
            HashMap::accessor acc;
            if (maps.find(acc, Key(computeGist(state, job), job)))
                return;
            if (maps.insert(acc, Key(computeGist(state, job), job)))
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

    void addDelayed(std::vector<int> gist, int job, tbb::task::suspend_point tag) override
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

    void logging(std::vector<int> state, int job, auto message = "")
    {
        if (!detailedLogging)
            return;
        std::stringstream gis;
        gis << message << " ";
        for (auto vla : state)
            gis << vla << ", ";
        gis << " => ";
        for (auto vla : Key(computeGist(state, job), job))
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
