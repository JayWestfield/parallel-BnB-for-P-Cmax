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
// !!!!!!! this is not maintained, until the suspend works !!!!!!!!!
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
        bool equal(const std::vector<int> &a, const std::vector<int> &b) const
        {
            return std::equal(a.begin(), a.end(), b.begin());
        }
    };
const size_t MAX_SIZE = 50000000;
const size_t BITMAP_SIZE = 50;
class STImpl : public ST
{
public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;
    using HashDelayedMap = tbb::concurrent_hash_map<std::vector<int>, std::vector<tbb::task::suspend_point>, VectorHasher>; // might have more than one suspension point for a gist

    STImpl(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : ST(jobSize, offset, RET, vec_size), maps(jobSize), delayedTasks(jobSize), bitmaps(jobSize, std::bitset<BITMAP_SIZE>())
    {
        initializeThreadLocalVector(vec_size);
    }
    ~STImpl()
    {
        std::unique_lock<std::shared_mutex> lock(clearLock);
    }
    std::vector<int> computeGist(const std::vector<int> &state, int job) override
    {
        // assume the state is sorted
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()) && vec_size == state.size());
        std::vector<int> gist(vec_size, 0);
        assert((long unsigned int)(state.back() + offset) < RET[job].size()); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < vec_size; i++)
        {
            gist[i] = RET[job][state[i] + offset];
        }
        return gist;
    }
    void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) override
    {
        initializeThreadLocalVector(vec_size);
        // assume the state is sorted
        assert(job < jobSize && job >= 0 && std::is_sorted(state.begin(), state.end()));

        assert(gist.size() >= vec_size);
        assert(gist.size() >= state.size());

        assert((long unsigned int)(state.back() + offset) < RET[job].size()); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < vec_size; i++)
        {
            gist[i] = RET[job][state[i] + offset];
        }
    }

    void addGist(const std::vector<int> &state, int job) override
    {
        assert(job < jobSize && job >= 0);

        if (maps[job].size() >= MAX_SIZE)
        {
            std::cout << "evict " << std::endl;
            evictEntries(job);
        }

        std::shared_lock<std::shared_mutex> lock(clearLock);
        if ((state.back() + offset) >= maximumRETIndex) return;

        computeGist2(state, job, threadLocalVector);

        HashMap::accessor acc;
        maps[job].insert(acc, threadLocalVector);
        acc->second = true;
        assert(acc->second == true);

        acc.release();
        HashDelayedMap::accessor accDel;
        if (delayedTasks[job].find(accDel, threadLocalVector))
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

        std::shared_lock<std::shared_mutex> lock(clearLock);
        if ((state.back() + offset) >= maximumRETIndex) return 2;
        if (job >= 0 && job < jobSize)
        {
            computeGist2(state, job, threadLocalVector);

            HashMap::const_accessor acc;
            if (maps[job].find(acc, threadLocalVector))
            {
                updateBitmap(job, threadLocalVector);
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }

    void addPreviously(const std::vector<int> &state, int job) override
    {
        std::shared_lock<std::shared_mutex> lock(clearLock);
        if ((state.back() + offset) >= maximumRETIndex) return;
        computeGist2(state, job, threadLocalVector);

        if (job >= 0 && job < jobSize)
        {
            HashMap::accessor acc;
            if (maps[job].find(acc, threadLocalVector))
                return;
            else
            {
                maps[job].insert(acc, threadLocalVector);
                acc->second = false;
            }
        }
        logging(state, job, "add Prev");
    }

    void boundUpdate(int offset) override
    {
        std::unique_lock<std::shared_mutex> lock(clearLock);

        if (offset <= this->offset)
            return;

        clear();
        int ele = 0;
        for (auto m : maps) {
            ele += m.size();
        }

        this->offset = offset;
    }
        void prepareBoundUpdate() override
    {
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
        // threadLocalVector.resize(0);
        // threadLocalVector.clear();
    }

    void resumeAllDelayedTasks() override
    {
        // std::cout << "resume " << delCount << std::endl;
        // needs an own lock because it is also called on cancel
        std::unique_lock<std::shared_mutex> lock(clearLock);

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
    void addDelayed(const std::vector<int> &state, int job, tbb::task::suspend_point tag) override
    {
        assert(job < jobSize && job >= 0);

        // std::cout << "add delay " << delCount << std::endl;

        std::shared_lock<std::shared_mutex> mapLock(clearLock);
        // std::cout << "add delay: lock " << delCount << std::endl;
        computeGist2(state, job, threadLocalVector);

        HashMap::const_accessor acc;
        if (maps[job].find(acc, threadLocalVector))
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
                delayedTasks[job].insert(acc, threadLocalVector);
                delCount++;
                acc->second.push_back(tag);
            }
        }
        else
        {
            tbb::task::resume(tag);
            return;
        } // TODO that should only happen on bound update or evict
        logging(state, job, "endDelay");
    }

private:
    bool detailedLogging = false;
    std::atomic<int> delCount = 0;
    std::vector<HashMap> maps;
    std::vector<HashDelayedMap> delayedTasks;
    std::vector<std::bitset<BITMAP_SIZE>> bitmaps;
    std::shared_mutex mapsLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    bool useBitmaps = false;
    std::shared_mutex delayed;   // shared_mutex zum Schutz der Hashmaps während boundUpdate
    std::shared_mutex clearLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate

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
