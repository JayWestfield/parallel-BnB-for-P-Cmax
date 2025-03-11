#pragma once
#include "_refactoredBnB/ST/ST_combined.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
// note not really refactored this one
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
  size_t hash(int *a) const {
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  size_t operator()(int *a) const {
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  bool operator()(const int *lhs, const int *rhs) const {
    return std::equal(lhs, lhs + ws::gistLength, rhs);
  }
  inline bool equal(int *a, int *b) const {
    return std::equal(a, a + ws::gistLength, b);
  }
};
template <bool use_fingerprint> struct VectorHasherPrint {
  inline void hash_combine(std::size_t &s, const int &v) const {
    s ^= hashingCombined::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
  }
  size_t hash(int *a) const {
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  size_t operator()(int *a) const {
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, a[i]);
    }
    return h;
  }
  bool operator()(const int *lhs, const int *rhs) const {
    if (use_fingerprint &&
        FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer((lhs)) !=
            FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer((rhs)))
      return false;
    return std::equal(
        FingerPrintUtil<use_fingerprint>::getOriginalPointer((lhs)),
        FingerPrintUtil<use_fingerprint>::getOriginalPointer((lhs)) +
            ws::gistLength,
        FingerPrintUtil<use_fingerprint>::getOriginalPointer((rhs)));
  }
  inline bool equal(int *a, int *b) const {
    if (use_fingerprint &&
        FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(a) !=
            FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(b))
      return false;
    return std::equal(FingerPrintUtil<use_fingerprint>::getOriginalPointer(a),
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(a) +
                          ws::gistLength,
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(b));
  }
};
template <bool use_fingerprint> struct VectorHasherCast {
  using Key = int *;
  using StoreKey = uint64_t;
  inline void hash_combine(std::size_t &s, const int &v) const {
    s ^= hashingCombined::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
  }
  size_t hash(StoreKey a) {
    auto pointer = FingerPrintUtil<use_fingerprint>::getOriginalPointer(
        reinterpret_cast<Key>(a));
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, (pointer)[i]);
    }
    return h;
  }
  size_t operator()(StoreKey a) const {
    auto pointer = FingerPrintUtil<use_fingerprint>::getOriginalPointer(
        reinterpret_cast<Key>(a));
    size_t h = 17;
    for (int i = 0; i < ws::gistLength; i++) {
      hash_combine(h, pointer[i]);
    }
    return h;
  }
  // TODO add fingerprint comparison in the equal method
  bool operator()(const StoreKey lhs, const StoreKey rhs) const {
    if (use_fingerprint &&
        FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
            reinterpret_cast<ws::HashKey>(lhs)) !=
            FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
                reinterpret_cast<ws::HashKey>(rhs)))
      return false;
    return std::equal(FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(lhs)),
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(lhs)) +
                          ws::gistLength,
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(rhs)));
  }
  inline bool equal(StoreKey a, StoreKey b) const {
    if (use_fingerprint &&
        FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
            reinterpret_cast<ws::HashKey>(a)) !=
            FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
                reinterpret_cast<ws::HashKey>(b)))
      return false;
    return std::equal(FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(a)),
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(a)) +
                          ws::gistLength,
                      FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                          reinterpret_cast<ws::HashKey>(b)));
  }
};
}; // namespace hashingCombined
