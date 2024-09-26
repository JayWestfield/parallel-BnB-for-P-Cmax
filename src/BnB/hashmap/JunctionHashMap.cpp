// JunctionHashMap.h
#pragma once

#include "IConcurrentHashMap.h"
#include <junction/ConcurrentMap_Linear.h>
#include <turf/Util.h>
#include <vector>
#include <string>

class JunctionHashMap : public IConcurrentHashMap {
    struct VectorPtrKeyTraits {
    typedef std::vector<int>* Key;

    static size_t hash(const Key& key) {
        VectorHasher hasher;
        return hasher(*key);
    }

    static bool equals(const Key& a, const Key& b) {
        return *a == *b;
    }

    static const Key NullKey() {
        return nullptr; // Use nullptr as the null key
    }

    static const size_t NullHash = 0;
};
    junction::ConcurrentMap_Linear<std::vector<int>, int, VectorPtrKeyTraits> map_;

    uint64_t hashVector(const std::vector<int>& v) const {
        std::size_t hash = 0;
        for (int i : v) {
            hash ^= std::hash<int>{}(i) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return static_cast<uint64_t>(hash);
    }

public:
    void insert(const std::vector<int>& key, bool value) override {
        map_.assign(key, value ? 2 : 1);
    }

    int find(const std::vector<int>& key) override {
        bool value;
        auto it = map_.find(key);
        if (it.getValue()) {
            return it.getValue();  // Return 2 if true, 1 if false
        }
        return 0;
    }
    void clear() {
        map_ = junction::ConcurrentMap_Linear<std::vector<int>, int, VectorPtrKeyTraits>();
    }

    void iterate() const override {
        // Junction does not support iteration
    }
};
