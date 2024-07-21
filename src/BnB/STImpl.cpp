#ifndef ST_IMPL_H
#define ST_IMPL_H

#include "ST.h"
#include <tbb/tbb.h>
#include <iostream>


class STImpl : public ST {
public:
    using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher>;

    STImpl(int jobSize, std::atomic<int>* upperBound, std::atomic<int>* offset, std::vector<std::vector<int>>* RET) : ST(jobSize, upperBound, offset, RET), maps(jobSize) {}
    // assume the state is sorted
    std::vector<int> computeGist(std::vector<int> state, int job) override {
        std::vector<int> gist(state.size());
        const int off = *offset; 
        for (long unsigned int i = 0; i < state.size(); i++ ) {
            gist[i] = (*RET)[job][state[i] + off];
        }
        return gist;
    }

    void addGist(std::vector<int> state, int job) override {
        if (maps[job].size()  >= 500000) {
            tbb::concurrent_hash_map<std::vector<int>, bool, VectorHasher> newMap;
            maps[job].swap(newMap);
        }
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (job >= 0 && job < jobSize) {
            HashMap::accessor acc;
            maps[job].insert(acc, computeGist(state, job));
            acc->second = true;
        }
    } 

    unsigned char exists(std::vector<int> state, int job) override {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (job >= 0 && job < jobSize) {
            HashMap::const_accessor acc;
            if (maps[job].find(acc, computeGist(state, job))) {
                return acc->second ? 2 : 1;
            }
        }
        return 0;
    }

    void addPreviously(std::vector<int> state, int job) override {
        std::shared_lock<std::shared_mutex> lock(mutex);
        if (job >= 0 && job < jobSize) {
            HashMap::accessor acc;
            maps[job].insert(acc, computeGist(state, job));
            acc->second = false;
        }
    }

    void boundUpdate() override {
        // Sperre die Mutex exklusiv, um parallele Zugriffe zu verhindern
        std::unique_lock<std::shared_mutex> lock(mutex);
        std::vector<HashMap> newMaps(jobSize);
        maps.swap(newMaps);
    }

private:
    std::vector<HashMap> maps;
    std::shared_mutex mutex; // shared_mutex zum Schutz der Hashmaps während boundUpdate
};

#endif // ST_IMPL_H
