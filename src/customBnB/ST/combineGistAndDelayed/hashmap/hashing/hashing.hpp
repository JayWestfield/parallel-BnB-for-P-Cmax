#ifndef hashingCombined_H
#define hashingCombined_H
#include "../../../../globalValues.cpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
extern int gistLength;

namespace hashingCombined {

// source:
// https://github.com/domschrei/mallob/blob/6a9bea6955b6c0c1b21826aba19842bb17e1e1f3/src/util/robin_hood.hpp
inline size_t hash_int(uint64_t x) noexcept {

  x ^= x >> 33U;
  x *= UINT64_C(0xff51afd7ed558ccd);
  x ^= x >> 33U;

  // not doing the final step here, because this will be done by keyToIdx
  // anyways x *= UINT64_C(0xc4ceb9fe1a85ec53); x ^= x >> 33U;
  return static_cast<size_t>(x);
}
struct VectorHasher {
  inline void hash_combine(std::size_t &s, const int &v) const {
    s ^= hashingCombined::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
  }
  size_t hash(int *a) {
    size_t h = 17;
    for (int i = 0; i < gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  size_t operator()(int *a) const {
    size_t h = 17;
    for (int i = 0; i < gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  bool operator()(const int *lhs, const int *rhs) const {
    return std::equal(lhs, lhs + gistLength, rhs);
  }
  inline bool equal(int *a, int *b) const {
    return std::equal(a, a + gistLength, b);
  }
};

struct VectorHasherCast {
  using Key = int *;
  using StoreKey = long long unsigned int; 
  inline void hash_combine(std::size_t &s, const int &v) const {
    s ^= hashingCombined::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
  }
  size_t hash(StoreKey a) {
    size_t h = 17;
    for (int i = 0; i < gistLength; i++) {
      hash_combine(h, reinterpret_cast<Key>(a)[i]);
    }
    return h;
  }
  size_t operator()(StoreKey a) const {
    size_t h = 17;
    for (int i = 0; i < gistLength; i++) {
      hash_combine(h, reinterpret_cast<Key>(a)[i]);
    }
    return h;
  }
  bool operator()(const StoreKey lhs, const StoreKey rhs) const {
    return std::equal(reinterpret_cast<Key>(lhs), reinterpret_cast<Key>(lhs) + gistLength, reinterpret_cast<Key>(rhs));
  }
  inline bool equal(StoreKey a, StoreKey b) const {
    return std::equal(reinterpret_cast<Key>(a), reinterpret_cast<Key>(a) + gistLength, reinterpret_cast<Key>(b));
  }
};
}; // namespace hashingCombined
#endif