// IConcurrentHashMap.h
#ifndef IConcurrentHashMap_H
#define IConcurrentHashMap_H
#include <vector>
#include <string>
#include "../hashing/hashFuntions.hpp"
#include <cassert>
#include <tbb/tbb.h>

class IConcurrentHashMap
{
public:
    virtual ~IConcurrentHashMap() = default;
    struct VectorHasher
    {
        inline void hash_combine(std::size_t &s, const tbb::detail::d1::numa_node_id &v) const
        {
            s ^= hashing::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
        }
        size_t hash(const std::vector<tbb::detail::d1::numa_node_id> &vec)
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        size_t operator()(const std::vector<tbb::detail::d1::numa_node_id> &vec) const
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        bool equal(const std::vector<int> &a, const std::vector<int> &b) const
        {
            return std::equal(a.begin(), a.end(), b.begin());
        }
    };

    virtual void insert(const std::vector<int> &key, bool value) = 0;
    virtual int find(const std::vector<int> &key) = 0;
    virtual void iterate() const = 0;
    virtual void clear() = 0;
};

#endif