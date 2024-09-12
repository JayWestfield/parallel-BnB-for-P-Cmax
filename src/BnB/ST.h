#ifndef ST_H
#define ST_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>
#include <cassert>
#include "threadLocal/threadLocal.h"
#include "hashing/hashFuntions.hpp"

struct VectorHasher
{
    // hashing copied from Algorithm engineering course  https://gitlab.kit.edu/kit/iti/ae/teaching/algorithm-engineering/uebung/framework/-/blob/main/utils.h
    const inline std::uint64_t murmur_hash64(const std::vector<int> &vec)
    {
        assert(vec.size() == vec.size());
        const std::uint64_t m = 0xc6a4a7935bd1e995;
        const std::size_t seed = 1203989050u;
        const int r = 47;
        std::size_t len = vec.size() * sizeof(int);
        std::uint64_t h = seed ^ (len * m);

        const std::uint64_t *data = (const std::uint64_t *)vec.data();
        const std::uint64_t *end = data + (len / 8);

        while (data != end)
        {
            std::uint64_t k = *data++;

            k *= m;
            k ^= k >> r;
            k *= m;

            h ^= k;
            h *= m;
        }

        const unsigned char *data2 = reinterpret_cast<const unsigned char *>(data);

        switch (len & 7)
        {
        case 7:
            h ^= static_cast<std::int64_t>(data2[6]) << 48; // fallthrough
        case 6:
            h ^= static_cast<std::uint64_t>(data2[5]) << 40; // fallthrough
        case 5:
            h ^= static_cast<std::uint64_t>(data2[4]) << 32; // fallthrough
        case 4:
            h ^= static_cast<std::uint64_t>(data2[3]) << 24; // fallthrough
        case 3:
            h ^= static_cast<std::uint64_t>(data2[2]) << 16; // fallthrough
        case 2:
            h ^= static_cast<std::uint64_t>(data2[1]) << 8; // fallthrough
        case 1:
            h ^= static_cast<std::uint64_t>(data2[0]);
            h *= m;
        };

        h ^= h >> r;
        h *= m;
        h ^= h >> r;

        return h;
    }
    inline void hash_combine(std::size_t &s, const tbb::detail::d1::numa_node_id &v)
    {
        s ^= hashing::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
    }
    size_t hash(const std::vector<tbb::detail::d1::numa_node_id> &vec)
    {
        size_t h = 17;
        for (auto entry : vec) {
            hash_combine(h, entry);
        }
        return h;
        // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
        //return murmur_hash64(vec);
    }
    bool equal(const std::vector<int> &a, const std::vector<int> &b) const
    {
        return std::equal(a.begin(), a.end(), b.begin());
    }
};
class ST
{
public:
    // Konstruktor mit Parameter int jobSize
    ST(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : jobSize(jobSize), offset(offset), RET(RET), vec_size(vec_size), maximumRETIndex(RET[0].size())
    {
    }

    virtual ~ST() = default;

    virtual void addGist(const std::vector<int> &gist, int job) = 0;
    virtual int exists(const std::vector<int> &, int job) = 0;
    virtual void addPreviously(const std::vector<int> &gist, int job) = 0;
    virtual void boundUpdate(int offset) = 0;
    virtual std::vector<int> computeGist(const std::vector<int> &state, int job) = 0;
    virtual void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) = 0;
    virtual void addDelayed(const std::vector<int> &gist, int job, oneapi::tbb::task::suspend_point tag) = 0;
    virtual void resumeAllDelayedTasks() = 0;
    virtual void clear() = 0;
    // ob das  funktioniert?
    // static thread_local std::vector<int> threadLocalVector;

protected:
    int jobSize; // Member-Variable zum Speichern der jobSize
    int offset;
    const std::vector<std::vector<int>> &RET;
    std::size_t vec_size;
    int maximumRETIndex;
};

#endif // ST_H
