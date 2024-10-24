#include "IConcurrentHashMap.h"
#include <tbb/concurrent_hash_map.h>
#include <vector>
#include <iostream>


class TBBHashMap : public IConcurrentHashMap {
    using Key = std::vector<int>;
    using Value = bool;

    tbb::concurrent_hash_map<Key, Value, hashing::VectorHasher> map_;

public:
    void insert(const Key& key, Value value) override {
        tbb::concurrent_hash_map<Key, Value, hashing::VectorHasher>::accessor accessor;
        map_.insert(accessor, key);
        accessor->second = accessor->second || value;
    }

    int find(const Key& key) override {
        tbb::concurrent_hash_map<Key, Value, hashing::VectorHasher>::const_accessor accessor;
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
