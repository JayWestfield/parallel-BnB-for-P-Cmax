#ifndef ST_IMPL_H
#define ST_IMPL_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>
#include <bitset>


const size_t MAX_SIZE = 5000000;
const size_t BITMAP_SIZE = MAX_SIZE;

class STImpl : public ST {
public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;
    using HashDelayedMap = tbb::concurrent_hash_map<std::vector<int>, std::vector<oneapi::tbb::task::suspend_point>, VectorHasher>; // might have more than one suspension point for a gist

    STImpl(int jobSize, std::atomic<int>* upperBound, std::atomic<int>* offset, std::vector<std::vector<int>>* RET, std::shared_mutex *boundLock) : ST(jobSize, upperBound, offset, RET, boundLock), maps(jobSize),delayedTasks(jobSize), bitmaps(jobSize, std::bitset<BITMAP_SIZE>()) {}
    // assume the state is sorted
    std::vector<int> computeGist(std::vector<int> state, int job) override {
        std::shared_lock lock(boundLock);
        std::sort(state.begin(), state.end());
        std::vector<int> gist(state.size(), 0);
        if (state.back() > *upperBound) return gist; // for the case that the boundupdate makes the instance invalid
        // for (auto val: state) std::cout << val << " ";
        const int off = offset->load(std::memory_order_acquire);
        // std::cout <<  " => "; 
        for (int i = 0; i < state.size(); i++ ) {
            gist[i] = (*RET)[job][state[i] + off];
        }
        // for (auto val: gist) std::cout << val << " ";
        // std::cout << std::endl;

        return gist;
    }

    void addGist(std::vector<int> state, int job) override {
        if (maps[job].size() >= MAX_SIZE) {
            std::unique_lock<std::shared_mutex> lock(mutex); // todo combine try lock with tthe if condition
            std::cout << "Evict " << std::endl;
            evictEntries(job);
            lock.unlock();
        }

        std::shared_lock<std::shared_mutex> lock(mutex);
                // std::cout << job << " ";

        if (job >= 0 && job < jobSize) {
            HashMap::accessor acc;
            maps[job].insert(acc, computeGist(state, job));
            acc->second = true;
        }
        std::unique_lock<std::shared_mutex> delayLock(delayed); // do i even need the lock with the accessor or just the shared one because of the clear
        HashDelayedMap::accessor acc;
        if (delayedTasks[job].find(acc, computeGist(state, job))) {
            for (auto tag : acc->second) {oneapi::tbb::task::resume(tag); delCount--;}
            std::cout << "during add "<< delCount << std::endl;
            delayedTasks[job].erase(acc);
            int count = 0;
                HashMap::const_accessor acc;

                for (int i = 0; i < jobSize; i++) {
                    for(auto entry: delayedTasks[i]) {
                        count += entry.second.size();
                        for (auto t : entry.first) std::cout << t << " " ;
                        if (maps[i].find(acc, entry.first))                        std::cout <<  "finder " <<acc->second << std::endl;
                        else std::cout << "notfound ata ll" << std::endl;
                    }
                }
                std::cout << "in delayed " << count  << " vs delcount "<< delCount <<std::endl;
            
        }       
        
        delayLock.unlock();
    } 

    int exists(std::vector<int> state, int job) override {
        // std::cout << job << " ";
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
            if(maps[job].insert(acc, computeGist(state, job))) acc->second = false;
            acc.release();
        }
    }

    void boundUpdate() override {

        // Sperre die Mutex exklusiv, um parallele Zugriffe zu verhindern
        std::unique_lock<std::shared_mutex> lock(mutex);
        resumeAllDelayedTasks(); // TODO does thos need the other lock?
        std::vector<HashMap> newMaps(jobSize);
        maps.swap(newMaps);
        if (useBitmaps) {
            for (auto& bitmap : bitmaps) {
                bitmap.reset();
            }
        }
        clear();
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
        for (auto map : maps) {
            for (auto et : map) {
                std::cout << et.second << "asd" << std::endl;
            }
        }
    }
    void resumeAllDelayedTasks() override{
        std::unique_lock<std::shared_mutex> lock(delayed);
        for (auto& delayedMap : delayedTasks) {
                for (auto it :delayedMap) {
                    for (auto tag : it.second) {oneapi::tbb::task::resume(tag);
                    delCount--;}
                }
            }

        std::vector<HashDelayedMap> newMaps(jobSize);
        delayedTasks.swap(newMaps);
    }


    void addDelayed(std::vector<int> gist, int job, oneapi::tbb::task::suspend_point tag) override {
        std::unique_lock<std::shared_mutex> lockU(allLock);

        std::unique_lock<std::shared_mutex> lock(delayed);
        HashMap::const_accessor acc;
        if (maps[job].find(acc, computeGist(gist, job))) { //can that even happen? yes it can and does
            if (acc->second){
                tbb::task::resume(tag);
                return;
            }  
        }
        if (job >= 0 && job < jobSize) {
            HashDelayedMap::accessor acc;
            delayedTasks[job].insert(acc, computeGist(gist, job));
            delCount++;
            acc->second.push_back(tag);
        } else {std::cout << "rollasdasdasd" ;}
    }

private:
    std::atomic<int> delCount = 0;
    std::vector<HashMap> maps;
    std::shared_mutex mutex; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    std::vector<std::bitset<BITMAP_SIZE>> bitmaps;
    std::vector<HashDelayedMap> delayedTasks;
    bool useBitmaps = false;
    std::shared_mutex delayed; // shared_mutex zum Schutz der Hashmaps während boundUpdate
    std::vector<oneapi::tbb::task::suspend_point> delayedTa;
    std::shared_mutex allLock; // shared_mutex zum Schutz der Hashmaps während boundUpdate

    void updateBitmap(int job, const std::vector<int>& gist) {
        if (useBitmaps) {
            size_t hashValue = VectorHasher().hash(gist) % BITMAP_SIZE;
            bitmaps[job].set(hashValue);
        }
    }
    //TODO look for deadlocks etc
    void evictEntries(int job) {
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
};

#endif // ST_IMPL_H
