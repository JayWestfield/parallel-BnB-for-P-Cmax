// GrowtHashMap.h
#pragma once

#include "IConcurrentHashMap.h"
#include <tbb/concurrent_hash_map.h>
#include <vector>
#include <string>
#include <iostream>
#include "data-structures/table_config.hpp"
#include "allocator/alignedallocator.hpp"

class GrowtHashMap : public IConcurrentHashMap
{
    
    using Key = std::vector<int>;
    using Value = bool;

    using allocator_type = growt::AlignedAllocator<bool>;

    using HashMap = typename growt::table_config<std::vector<tbb::detail::d1::numa_node_id>, bool, VectorHasher, allocator_type,
                                                 hmod::growable, hmod::deletion>::table_type;
    struct boolWriteTrue
    {
        using mapped_type = bool;

        mapped_type operator()(mapped_type &lhs, const mapped_type &rhs) const
        {
            lhs = rhs;
            return rhs;
        }

        // an atomic implementation can improve the performance of updates in .sGrow
        // this will be detected automatically
        mapped_type atomic(mapped_type &lhs, const mapped_type &rhs) const
        {

            lhs |= rhs;
            return lhs |= rhs;
        }

        // Only necessary for JunctionWrapper (not needed)
        using junction_compatible = std::true_type;
    };
    HashMap map_;

public:
GrowtHashMap(): map_(2000)
    {
    };
    void insert(const Key &key, Value value) override
    {
        auto handle = map_.get_handle();
        handle.insert_or_update(key, true, boolWriteTrue(), true);
    }

    int find(const Key &key) override
    {
        auto handle = map_.get_handle();
        auto temp = handle.find(key);
        int result = 0;
        if (temp != handle.end())
        {
            result = (*temp).second ? 2 : 1;
        };
        return result;
    }

    void clear() override
    {
        map_ = HashMap(50);
    }

    void iterate() const override
    {
    }
};
