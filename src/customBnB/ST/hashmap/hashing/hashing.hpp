#ifndef hashing_H
#define hashing_H
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace hashing {
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

class VectorHasher {
  static inline void hash_combine(std::size_t &s, const int &v) {
    s ^= hashing::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
  }
  size_t hash(const std::vector<int> &vec) {
    size_t h = 17;
    for (auto entry : vec) {
      hash_combine(h, entry);
    }
    return h;
  }
  size_t operator()(const std::vector<int> &vec) const {
    size_t h = 17;
    for (auto entry : vec) {
      VectorHasher::hash_combine(h, entry);
    }
    return h;
  }
  bool operator()(const std::vector<int>& a, const std::vector<int>& b) const {
        return equal(a, b);
    }
  bool equal(const std::vector<int> &a, const std::vector<int> &b) const {
    return std::equal(a.begin(), a.end(), b.begin());
  }
};
}; // namespace hashing
#endif