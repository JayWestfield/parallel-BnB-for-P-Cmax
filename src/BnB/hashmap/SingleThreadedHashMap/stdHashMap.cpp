// stdHashMap.h
#pragma once

#include "ISingleThreadedHashMap.h"
#include <vector>
#include <string>
#include <iostream>


class stdHashMap : public ISingleThreadedHashMap {
    using Key = std::vector<int>;
    using Value = bool;

    std::unordered_map<Key, Value, hashing::VectorHasher> map_;

public:
    
    void insert(const Key& key) override {
        return;
        map_.clear();
        if (map_.size() > 5000) map_.clear();
        map_.insert({key, true});
    }

    bool find(const Key& key) override {
        return map_.find(key) != map_.end();
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
