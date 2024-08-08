#ifndef ST_IMPL_H
#define ST_IMPL_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>
#include <sstream>
#include <cassert>

const size_t MAX_SIZE = 5000000;
const size_t BITMAP_SIZE = MAX_SIZE;

class STImpl : public ST {
public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;
    using HashDelayedMap = tbb::concurrent_hash_map<std::vector<int>, std::vector<oneapi::tbb::task::suspend_point>, VectorHasher>; // might have more than one suspension point for a gist

    STImpl(int jobSize, int offset, std::vector<std::vector<int>>* RET) :  ST(jobSize, offset, RET),maps(jobSize),delayedTasks(jobSize), bitmaps(jobSize, std::bitset<BITMAP_SIZE>()) {}
    // assume the state is sorted
    std::vector<int> computeGist(std::vector<int> state, int job) override {
        assert(job < jobSize && job >= 0);
        std::sort(state.begin(), state.end());
        std::vector<int> gist(state.size(), 0);
        assert((long unsigned int)(state.back() + offset) < (*RET)[job].size()); // TODO maybe need error Handling to check that
        for (std::vector<int>::size_type i = 0; i < state.size(); i++ ) {
            gist[i] = (*RET)[job][state[i] + offset];
        }


        return gist;
    }

    void addGist(std::vector<int> state, int job) override {
        assert(job < jobSize && job >= 0);

        if (maps[job].size() >= MAX_SIZE) {
            std::cout << "Evict " << std::endl;
            evictEntries(job);
        }

        std::shared_lock<std::shared_mutex> lock(updateBound);

        HashMap::accessor acc;
        maps[job].insert(acc, computeGist(state, job));
        acc->second = true;
        assert(acc->second == true);
        std::stringstream gise;
        gise << " inserted Gist ";
        for (auto vla : computeGist(state, job)) gise << vla << " ";

        acc.release();
        gise << exists(state, job);
                gise << "\n";

        std::cout << gise.str();
        
        HashDelayedMap::accessor accDel;
        if (delayedTasks[job].find(accDel, computeGist(state, job))) {
            for (auto tag : accDel->second) {oneapi::tbb::task::resume(tag); delCount--;}
            delayedTasks[job].erase(accDel);
        }    
        std::cout << "resumedGists" << std::endl;
    } 

    int exists(std::vector<int> state, int job) override {
        assert(job < jobSize);

        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize) {
            HashMap::const_accessor acc;
            if (maps[job].find(acc, computeGist(state, job))) {
                updateBitmap(job, computeGist(state, job));
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }

    void addPreviously(std::vector<int> state, int job) override {
        std::shared_lock<std::shared_mutex> lock(updateBound);
        if (job >= 0 && job < jobSize) {
            HashMap::accessor acc;
            if (maps[job].find(acc, computeGist(state, job))) return;
            if(maps[job].insert(acc, computeGist(state, job))) acc->second = false;
        }
        std::stringstream gis;
        gis << "add Prev ";
        for (auto vla : computeGist(state, job)) gis << vla << " ";
        gis << "\n";
        std::cout << gis.str();
    }

    void boundUpdate(int offset) override {
        std::unique_lock<std::shared_mutex> lock(updateBound);
        std::cout << "bound" << std::endl;
        this->offset = offset;
        clear();
        resumeAllDelayedTasks(); // TODO does thos need the other lock?
        lock.unlock();
    }
    void clear() override {
        std::vector<HashMap> newMaps(jobSize);
        maps.swap(newMaps);
        if (useBitmaps) {
            for (auto& bitmap : bitmaps) {
                bitmap.reset();
            }
        }
    }
    void resumeAllDelayedTasks() override {
        std::cout << "resume " << delCount << std::endl;

        for (auto& delayedMap : delayedTasks) {
                for (auto it :delayedMap) {
                    for (auto tag : it.second) {oneapi::tbb::task::resume(tag);
                    delCount--;}
                }
        } 
        std::vector<HashDelayedMap> newMaps(jobSize);
        std::cout << "resumed " << delCount << std::endl;
        delayedTasks.swap(newMaps);
    }


    void addDelayed(std::vector<int> gist, int job, oneapi::tbb::task::suspend_point tag) override {
        assert(job < jobSize);

        std::cout << "add delay " << delCount << std::endl;

        std::shared_lock<std::shared_mutex> mapLock(updateBound);
                std::cout << "add delay: lock " << delCount << std::endl;

        HashMap::const_accessor acc;
        if (maps[job].find(acc, computeGist(gist, job))) { 
                std::cout << "add delay: find " << delCount << std::endl;
            if (acc->second){
                tbb::task::resume(tag);
                std::cout << "instantly resumed " << delCount << std::endl;
                return;
            }  else {
                HashDelayedMap::accessor acc;
                delayedTasks[job].insert(acc, computeGist(gist, job));
                delCount++;
                acc->second.push_back(tag);
            }
        } else {tbb::task::resume(tag); std::cout << "intre" << std::endl; return;} //TODO that should only happen on bound update or evict and should not be an issue
            
        std::stringstream gis;
        gis << "end delay ";
        for (auto vla : computeGist(gist, job)) gis << vla << " ";
        gis << "\n";
        std::cout << gis.str();
    }

private:
    std::atomic<int> delCount = 0;
    std::vector<HashMap> maps;
    std::vector<HashDelayedMap> delayedTasks;
    std::vector<std::bitset<BITMAP_SIZE>> bitmaps;
    std::shared_mutex mapsLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    bool useBitmaps = false;
    std::shared_mutex delayed; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    std::vector<oneapi::tbb::task::suspend_point> delayedTa;
    std::shared_mutex updateBound; // shared_mutex zum Schutz der Hashmaps während boundUpdate

    void updateBitmap(int job, const std::vector<int>& gist) {
        assert(job < jobSize);
        if (useBitmaps) {
            size_t hashValue = VectorHasher().hash(gist) % BITMAP_SIZE;
            bitmaps[job].set(hashValue);
        }
    }
    //TODO look for deadlocks etc
    void evictEntries(int job) {
                std::cout << "evicte " << std::endl;

        std::unique_lock<std::shared_mutex> lock(mapsLock); // todo combine try lock with tthe if condition
        auto& map = maps[job];
        HashMap newMap;
        if (useBitmaps) {
            auto& bitmap = bitmaps[job];
            std::bitset<BITMAP_SIZE> newBitmap;
            for (auto it = map.begin(); it != map.end(); ++it) {
                size_t hashValue = VectorHasher().hash(it->first) % BITMAP_SIZE;
                if (bitmap.test(hashValue)) {
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
