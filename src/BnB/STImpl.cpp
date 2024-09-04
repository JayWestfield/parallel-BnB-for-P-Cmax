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

const size_t MAX_SIZE = 50000000;
const size_t BITMAP_SIZE = 50;

class STImpl : public ST
{
public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;
    using HashDelayedMap = tbb::concurrent_hash_map<std::vector<int>, std::vector<tbb::task::suspend_point>, VectorHasher>; // might have more than one suspension point for a gist

    STImpl(int jobSize, int offset, std::vector<std::vector<int>> *RET) : ST(jobSize, offset, RET), maps(jobSize), delayedTasks(jobSize), bitmaps(jobSize, std::bitset<BITMAP_SIZE>()) {}
    std::vector<int> computeGist(const std::vector<int> &state, int job) override
    {
        // assume the state is sorted
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));
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

    void addGist(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize && job >= 0);
        if (getMemoryUsagePercentage() > 80)
        {
            std::random_device rd;                                   // obtain a random number from hardware
            std::mt19937 gen(rd());                                  // seed the generator
            std::uniform_int_distribution<> distr(0, this->jobSize); // define the range
            const int other = distr(gen);
            std::cout << "skipped " << maps[job].size() << " " << maps[other].size() << std::endl;
            double memoryUsage = getMemoryUsagePercentage();
            std::cout << "Memory usage high: " << memoryUsage << "%. Calling evictAll." << std::endl;
            evictAll();

            // Überprüfe die Speicherauslastung nach evictAll
            memoryUsage = getMemoryUsagePercentage();
            std::cout << "Memory usage after evictAll: " << memoryUsage << "%" << std::endl;

            return;
        }
        if (maps[job].size() >= MAX_SIZE)
        {
            std::cout << "evict " << std::endl;
            evictEntries(job);
        }

        std::shared_lock<std::shared_mutex> lock(updateBound);

        HashMap::accessor acc;
        maps[job].insert(acc, computeGist(state, job));
        acc->second = true;
        assert(acc->second == true);

        acc.release();
        HashDelayedMap::accessor accDel;
        if (delayedTasks[job].find(accDel, computeGist(state, job)))
        {
            for (auto tag : accDel->second)
            {
                tbb::task::resume(tag);
                delCount--;
            }
            delayedTasks[job].erase(accDel);
        }
        logging(state, job, "inserted Gist " + delCount);
    }

    int exists(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize);

        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize)
        {
            HashMap::const_accessor acc;
            if (maps[job].find(acc, computeGist(state, job)))
            {
                updateBitmap(job, computeGist(state, job));
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
            HashMap::accessor acc;
            if (maps[job].find(acc, computeGist(state, job)))
                return;
            if (maps[job].insert(acc, computeGist(state, job)))
                acc->second = false;
        }
        logging(state, job, "add Prev");
    }

    void boundUpdate(int offset) override
    {

        std::unique_lock<std::shared_mutex> lock(updateBound);
        this->offset = offset;
        clear();
        resumeAllDelayedTasksInternally(); // TODO does thos need the other lock?
    }
    void clear() override
    {
        std::vector<HashMap> newMaps(jobSize);
        maps.swap(newMaps);
        if (useBitmaps)
        {
            for (auto &bitmap : bitmaps)
            {
                bitmap.reset();
            }
        }
    }

    void resumeAllDelayedTasks() override
    {
        // std::cout << "resume " << delCount << std::endl;
        // needs an own lock because it is also called on cancel
        std::unique_lock<std::shared_mutex> lock(updateBound);

        for (auto &delayedMap : delayedTasks)
        {
            for (auto it : delayedMap)
            {
                for (auto tag : it.second)
                {
                    tbb::task::resume(tag);
                    delCount--;
                }
            }
            // auto it = delayedMap.begin();
            // while (it != delayedMap.end()) {
            //      HashDelayedMap::accessor acc;
            //     if (delayedMap.find(acc, it->first)) {
            //         for (auto tag : acc->second) {
            //             tbb::task::resume(tag);
            //             delCount--;
            //         }
            //     }
            //     ++it;
            // }
        }
        std::vector<HashDelayedMap> newMaps(jobSize);
        // std::cout << "resumed " << delCount << std::endl;
        delayedTasks.swap(newMaps);
    }

    void addDelayed(const std::vector<int> &gist, int job, tbb::task::suspend_point tag) override
    {
        assert(job < jobSize && job >= 0);

        // std::cout << "add delay " << delCount << std::endl;

        std::shared_lock<std::shared_mutex> mapLock(updateBound);
        // std::cout << "add delay: lock " << delCount << std::endl;

        HashMap::const_accessor acc;
        if (maps[job].find(acc, computeGist(gist, job)))
        {
            // std::cout << "add delay: find " << delCount << std::endl;
            if (acc->second)
            {
                tbb::task::resume(tag);
                // std::cout << "instantly resumed " << delCount << std::endl;
                return;
            }
            else
            {
                HashDelayedMap::accessor acc;
                delayedTasks[job].insert(acc, computeGist(gist, job));
                delCount++;
                acc->second.push_back(tag);
            }
        }
        else
        {
            tbb::task::resume(tag);
            std::cout << "intre" << std::endl;
            return;
        } // TODO that should only happen on bound update or evict
        logging(gist, job, "endDelay");
    }

private:
    bool detailedLogging = false;
    std::atomic<int> delCount = 0;
    std::vector<HashMap> maps;
    std::vector<HashDelayedMap> delayedTasks;
    std::vector<std::bitset<BITMAP_SIZE>> bitmaps;
    std::shared_mutex mapsLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    bool useBitmaps = false;
    std::shared_mutex delayed;     // shared_mutex zum Schutz der Hashmaps während boundUpdate
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
    void updateBitmap(int job, const std::vector<int> &gist)
    {
        assert(job < jobSize && job >= 0);
        if (useBitmaps)
        {
            size_t hashValue = VectorHasher().hash(gist) % BITMAP_SIZE;
            bitmaps[job].set(hashValue);
        }
    }
    void resumeAllDelayedTasksInternally()
    {
        for (auto &delayedMap : delayedTasks)
        {
            for (auto it : delayedMap)
            {
                for (auto tag : it.second)
                {
                    tbb::task::resume(tag);
                    delCount--;
                }
            }
            // auto it = delayedMap.begin();
            // while (it != delayedMap.end()) {
            //      HashDelayedMap::accessor acc;
            //     if (delayedMap.find(acc, it->first)) {
            //         for (auto tag : acc->second) {
            //             tbb::task::resume(tag);
            //             delCount--;
            //         }
            //     }
            //     ++it;
            // }
        }
        std::vector<HashDelayedMap> newMaps(jobSize);
        // std::cout << "resumed " << delCount << std::endl;
        delayedTasks.swap(newMaps);
    }
    void logging(const std::vector<int> &state, int job, auto message = "")
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
    // TODO look for deadlocks etc
    void evictEntries(int job)
    {
        assert(job < jobSize && job >= 0);
        std::unique_lock<std::shared_mutex> lock(mapsLock); // todo combine try lock with tthe if condition
        auto &map = maps[job];
        HashMap newMap;
        if (useBitmaps)
        {
            auto &bitmap = bitmaps[job];
            std::bitset<BITMAP_SIZE> newBitmap;
            for (auto it = map.begin(); it != map.end(); ++it)
            {
                size_t hashValue = VectorHasher().hash(it->first) % BITMAP_SIZE;
                if (bitmap.test(hashValue))
                {
                    newMap.insert(*it);
                }
            }
            bitmap = newBitmap;
        }
        map.swap(newMap);
    }
    void evictAll()
    {
        std::unique_lock<std::shared_mutex> lock(mapsLock); // todo combine try lock with tthe if condition
        for (auto job = 0; job < jobSize; job++)
        {
            auto &map = maps[job];
            HashMap newMap;
            if (useBitmaps)
            {
                auto &bitmap = bitmaps[job];
                std::bitset<BITMAP_SIZE> newBitmap;
                for (auto it = map.begin(); it != map.end(); ++it)
                {
                    size_t hashValue = VectorHasher().hash(it->first) % BITMAP_SIZE;
                    if (bitmap.test(hashValue))
                    {
                        newMap.insert(*it);
                    }
                }
                bitmap = newBitmap;
            }
            map.swap(newMap);
        }
    }
    // void printHashDelayedMap() {
    //     std::ostringstream oss;
    //     std::cout << "start print" << std::endl;

    //     std::unique_lock<std::shared_mutex> mapLock(mapsLock);
    //     std::unique_lock<std::shared_mutex> lock(delayed);
    //     for (long unsigned int i = 0; i < delayedTasks.size(); i++) {
    //         for (const auto& item : delayedTasks[i]) {
    //             std::cout << "newGist\n";
    //             oss << "Gist: ";
    //             for (int val : item.first) {
    //                 oss << val << " ";
    //             }
    //             std::cout << "look\n";

    //             HashMap::const_accessor acc;
    //             oss << "found "<< maps[i].find(acc, computeGist(item.first, i)) ;
    //             if (maps[i].find(acc, computeGist(item.first, i))) oss << " flag " << acc->second;
    //             oss << "\n";
    //             std::cout << "look\n";
    //         }
    //     }

    //     // Output the entire constructed string at once
    //     std::cout << oss.str();
    // }
};

#endif // ST_IMPL_H
