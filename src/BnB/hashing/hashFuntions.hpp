#ifndef hashing_H
#define hashing_H
#include <cstdint>
#include <cstdlib>

namespace hashing
{
    // source: https://github.com/domschrei/mallob/blob/6a9bea6955b6c0c1b21826aba19842bb17e1e1f3/src/util/robin_hood.hpp
    inline size_t hash_int(uint64_t x) noexcept
    {

        x ^= x >> 33U;
        x *= UINT64_C(0xff51afd7ed558ccd);
        x ^= x >> 33U;

        // not doing the final step here, because this will be done by keyToIdx anyways
        // x *= UINT64_C(0xc4ceb9fe1a85ec53);
        // x ^= x >> 33U;
        return static_cast<size_t>(x);
    }
};
#endif