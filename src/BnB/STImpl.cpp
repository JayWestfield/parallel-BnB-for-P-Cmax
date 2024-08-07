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

    STImpl(int jobSize, std::atomic<int>* upperBound, std::atomic<int>* offset, std::vector<std::vector<int>>* RET, std::shared_mutex *boundLock) :  ST(jobSize, upperBound, offset, RET, boundLock),maps(jobSize),delayedTasks(jobSize), bitmaps(jobSize, std::bitset<BITMAP_SIZE>()) {}
    // assume the state is sorted
    std::vector<int> computeGist(std::vector<int> state, int job) override {
        assert(job < jobSize && job >= 0);
        std::shared_lock lock(boundLock);
        std::sort(state.begin(), state.end());
        std::vector<int> gist(state.size(), 0);
        const int off = offset->load(std::memory_order_acquire);
        if (state.back() + off > (*RET)[job].size()) throw ("invalid");
        for (std::vector<int>::size_type i = 0; i < state.size(); i++ ) {
            gist[i] = (*RET)[job][state[i] + off];
        }


        return gist;
    }

    void addGist(std::vector<int> state, int job) override {
        assert(job < jobSize && job >= 0);

        if (maps[job].size() >= MAX_SIZE) {
            std::cout << "Evict " << std::endl;
            evictEntries(job);
        }
        std::stringstream gis;
        gis << "add Gist ";
        for (auto vla : computeGist(state, job)) gis << vla << " ";
        gis << "\n";

        std::cout << gis.str();;

        std::shared_lock<std::shared_mutex> lock(mutex);

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
        
        std::shared_lock<std::shared_mutex> delayLock(delayed); // do i even need the lock with the accessor or just the shared one because of the clear
        HashDelayedMap::accessor accDel;
        if (delayedTasks[job].find(accDel, computeGist(state, job))) {
            for (auto tag : accDel->second) {oneapi::tbb::task::resume(tag); delCount--;}
            delayedTasks[job].erase(accDel);
        }    
        std::cout << delCount << std::endl;
    } 

    int exists(std::vector<int> state, int job) override {
        assert(job < jobSize);

        std::shared_lock<std::shared_mutex> lock(mutex);
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
        std::shared_lock<std::shared_mutex> lock(mutex);
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

    void boundUpdate() override {
        std::cout << "bound" << std::endl;
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::vector<HashMap> newMaps(jobSize);
        maps.swap(newMaps);
        if (useBitmaps) {
            for (auto& bitmap : bitmaps) {
                bitmap.reset();
            }
        }
        clear();
        resumeAllDelayedTasks(); // TODO does thos need the other lock?
        lock.unlock();
    }
    void clear() override {
        //std::unique_lock<std::shared_mutex> lock(mutex);
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

        std::unique_lock<std::shared_mutex> lock(delayed);
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

        std::shared_lock<std::shared_mutex> mapLock(mutex);
        std::shared_lock<std::shared_mutex> lock(delayed);
        HashMap::const_accessor acc;
        if (maps[job].find(acc, computeGist(gist, job))) { 
            if (acc->second){
                tbb::task::resume(tag);
                return;
            }  else {
                HashDelayedMap::accessor acc;
                delayedTasks[job].insert(acc, computeGist(gist, job));
                delCount++;
                acc->second.push_back(tag);
            }
        } else {tbb::task::resume(tag); std::cout << "intre"; return;} //TODO that should only happen on bound update or evict and should not be an issue
            
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
    std::shared_mutex mutex; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    bool useBitmaps = false;
    std::shared_mutex delayed; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    std::vector<oneapi::tbb::task::suspend_point> delayedTa;
    std::shared_mutex allLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate

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

        std::unique_lock<std::shared_mutex> lock(mutex); // todo combine try lock with tthe if condition
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
    void printHashDelayedMap() {
        std::ostringstream oss;
        std::cout << "start print" << std::endl;

        std::unique_lock<std::shared_mutex> mapLock(mutex);
        std::unique_lock<std::shared_mutex> lock(delayed);
        oss << "Bound: " << upperBound->load() << "\n";
        for (auto i = 0; i < delayedTasks.size(); i++) {
            for (const auto& item : delayedTasks[i]) {
                std::cout << "newGist\n";
                oss << "Gist: ";
                for (int val : item.first) {
                    oss << val << " ";
                }
                std::cout << "look\n";

                HashMap::const_accessor acc;
                oss << "found "<< maps[i].find(acc, computeGist(item.first, i)) ;
                if (maps[i].find(acc, computeGist(item.first, i))) oss << " flag " << acc->second;
                oss << "\n";
                std::cout << "look\n";
            }
        }

        // Output the entire constructed string at once
        std::cout << oss.str();
    }
};

#endif // ST_IMPL_H
