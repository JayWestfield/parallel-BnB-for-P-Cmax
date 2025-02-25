#include "_refactoredBnB/Structs/globalValues.hpp"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
// TODO sanity checks for this stuff since it can be tricky / error prone
// first bit might be used by growt
constexpr uintptr_t FINGERPRINT_MASK = 0x8000ffffffffffff;
constexpr uintptr_t MASK = 0x7FFF;
// Primary template: No fingerprint support
template <bool use_fingerprint> class FingerPrintUtil {
public:
  static uint16_t computeFingerprint(int *, size_t) {
    static_assert(use_fingerprint, "Fingerprinting is disabled.");
    return 0;
  }

  static int *tagPointerWithFingerprint(int *pointer, uint16_t) {
    return pointer;
  }
  static int *addFingerprint(int *value) {
    return tagPointerWithFingerprint(value, 0);
  }
  static uint16_t getFingerprintFromPointer(int *) { return 0; }

  static int *getOriginalPointer(int *ptr) {
    return ptr; // No tagging, just return ptr
  }
};
// TODO if it occurs in the profiler use a more efficient/easier  hash like xor
// or sth like that Specialization: Fingerprint support enabled
template <> class FingerPrintUtil<true> {
public:
  static uint16_t computeFingerprint(int *value) {
    uint32_t hash = 0x811C9DC5; // FNV-1a hash base
    // unroll???ß
    for (size_t i = 0; i < ws::gistLength; ++i) {
      hash ^= static_cast<int>(value[i]);
      hash *= 16777619u;
    }
    return static_cast<uint16_t>(hash & MASK);
  }

  static int *tagPointerWithFingerprint(int *ptr, uint16_t fingerprint) {
    uintptr_t rawPtr = reinterpret_cast<uintptr_t>(ptr);
    rawPtr = (rawPtr & FINGERPRINT_MASK) |
             (static_cast<uintptr_t>(fingerprint) << 48);
    return reinterpret_cast<int *>(rawPtr);
  }
  static int *addFingerprint(int *value) {
    return tagPointerWithFingerprint(value, computeFingerprint(value));
  }
  static uint16_t getFingerprintFromPointer(int *ptr) {
    return static_cast<uint16_t>((reinterpret_cast<uintptr_t>(ptr) >> 48) &
                                 MASK);
  }

  static int *getOriginalPointer(int *ptr) {
    return reinterpret_cast<int *>(reinterpret_cast<uintptr_t>(ptr) &
                                   FINGERPRINT_MASK);
  }
};
