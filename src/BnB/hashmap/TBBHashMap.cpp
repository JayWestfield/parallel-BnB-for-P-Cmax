// TBBHashMap.h
#pragma once

#include "IConcurrentHashMap.h"
#include <tbb/concurrent_hash_map.h>
#include <vector>
#include <string>
#include <iostream>


class TBBHashMap : public IConcurrentHashMap {
    using Key = std::vector<int>;
    using Value = bool;

    tbb::concurrent_hash_map<Key, Value, VectorHasher> map_;

public:
    void insert(const Key& key, Value value) override {
        tbb::concurrent_hash_map<Key, Value, VectorHasher>::accessor accessor;
        map_.insert(accessor, key);
        accessor->second |= value;
        std::stringstream gis;
        gis << "inserted gist" << " ";
        for (auto vla : key)
            gis << vla << ", ";
        gis << std::endl;
        std::cout << gis.str() << std::flush;
    }

    int find(const Key& key) override {
        tbb::concurrent_hash_map<Key, Value, VectorHasher>::const_accessor accessor;
        if (map_.find(accessor, key)) {
            return accessor->second ? 2 : 1;
        }
        return 0;
    }

    void clear() override {
        map_.clear();
    }

    void iterate() const override {
        for (const auto& pair : map_) {
            std::cout << "Key: {";
            for (int i : pair.first) {
                std::cout << i << " ";
            }
            std::cout << "} Value: " << std::boolalpha << pair.second << std::endl;
        }
    }
};
