// FollyHashMap.h
#pragma once

#include "IConcurrentHashMap.h"
#include <folly/concurrency/ConcurrentHashMap.h>
#include <vector>
#include <string>

class FollyHashMap : public IConcurrentHashMap {
    folly::ConcurrentHashMap<std::vector<int>, bool, VectorHasher> map_;

public:
    void insert(const std::vector<int>& key, bool value) override {
        map_.insert(key, value);
    }

    int find(const std::vector<int>& key) override {
        auto result = map_.find(key);
        if (result != map_.end()) {
            return result->second ? 2 : 1;
        }
        return 0;
    }

    void clear() override {
        map_.clear();
    }

    void iterate() const override {

    }
};
