#ifndef CUSTOM_UINT64_HPP
#define CUSTOM_UINT64_HPP

#include "_refactoredBnB/ST/ST_combined.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include <algorithm>
#include <cstdint>
#include <type_traits>
static constexpr size_t marked_bit = 1ull << 63;
static constexpr size_t bitmask = marked_bit - 1;
static constexpr size_t max = 0xffffffffffffffff;
template <bool use_fingerprint> struct CustomUint64 {
public:
  uint64_t value;
  using Key = int *;
  constexpr CustomUint64() : value(0) {}
  constexpr CustomUint64(uint64_t v) : value(v) {}

  // Vergleichsoperatoren
  bool operator==(const CustomUint64 &other) {
    // CustomUint64 oth = reinterpret_cast<CustomUint64>(other.value)
    if ((value & bitmask) == (max & bitmask))
      return this->value == other.value;
    else {
      if (use_fingerprint &&
          FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
              reinterpret_cast<int *>(value)) !=
              FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
                  reinterpret_cast<int *>(other.value)))
        return false;
      return std::equal(reinterpret_cast<Key>(value & bitmask),
                        reinterpret_cast<Key>(value & bitmask) + ws::gistLength,
                        reinterpret_cast<Key>(other.value & bitmask));
    }

    // return this->value == other.value;
  }

  constexpr bool operator==(const uint64_t &other) {
    if (other == 0)
      return this->value == other;
    else {
      if (use_fingerprint &&
          FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
              reinterpret_cast<int *>(value)) !=
              FingerPrintUtil<use_fingerprint>::getFingerprintFromPointer(
                  reinterpret_cast<int *>(value)))
        return false;
      return std::equal(FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                            reinterpret_cast<Key>(value & bitmask)),
                        FingerPrintUtil<use_fingerprint>::getOriginalPointer(
                            reinterpret_cast<Key>(value & bitmask)) +
                            ws::gistLength,
                        reinterpret_cast<Key>(other));
    }
  }

  constexpr bool operator<(const CustomUint64 &other) const {
    return this->value < other.value;
  }

  constexpr bool operator<=(const CustomUint64 &other) const {
    return this->value <= other.value;
  }

  constexpr bool operator>(const CustomUint64 &other) const {
    return this->value > other.value;
  }

  constexpr bool operator>=(const CustomUint64 &other) const {
    return this->value >= other.value;
  }

  // Konvertierungsoperator
  constexpr operator uint64_t() const { return value; }

  // Zuweisungsoperator
  constexpr CustomUint64 &operator=(uint64_t v) {
    value = v;
    return *this;
  }

  // Bitweise Operatoren
  constexpr inline CustomUint64 operator&(const uint64_t &other) const {
    return CustomUint64(value & other);
  }

  constexpr CustomUint64 operator|(const uint64_t &other) const {
    return CustomUint64(value | other);
  }

  constexpr CustomUint64 operator^(const uint64_t &other) const {
    return CustomUint64(value ^ other);
  }

  constexpr CustomUint64 operator~() const { return CustomUint64(~value); }

  constexpr CustomUint64 &operator&=(const uint64_t &other) {
    value &= other;
    return *this;
  }

  constexpr CustomUint64 &operator|=(const uint64_t &other) {
    value |= other;
    return *this;
  }

  constexpr CustomUint64 &operator^=(const uint64_t &other) {
    value ^= other;
    return *this;
  }

  // Verschiebeoperatoren
  constexpr CustomUint64 operator<<(const uint64_t &other) const {
    return CustomUint64(value << other);
  }

  constexpr CustomUint64 operator>>(const uint64_t &other) const {
    return CustomUint64(value >> other);
  }

  constexpr CustomUint64 &operator<<=(const uint64_t &other) {
    value <<= other;
    return *this;
  }

  constexpr CustomUint64 &operator>>=(const uint64_t &other) {
    value >>= other;
    return *this;
  }

  // Arithmetische Operatoren
  constexpr CustomUint64 operator+(const uint64_t &other) const {
    return CustomUint64(value + other);
  }

  constexpr CustomUint64 operator-(const uint64_t &other) const {
    return CustomUint64(value - other);
  }

  constexpr CustomUint64 operator*(const uint64_t &other) const {
    return CustomUint64(value * other);
  }

  constexpr CustomUint64 operator/(const uint64_t &other) const {
    return CustomUint64(value / other);
  }

  constexpr CustomUint64 operator%(const uint64_t &other) const {
    return CustomUint64(value % other);
  }

  constexpr CustomUint64 &operator+=(const uint64_t &other) {
    value += other;
    return *this;
  }

  constexpr CustomUint64 &operator-=(const uint64_t &other) {
    value -= other;
    return *this;
  }

  constexpr CustomUint64 &operator*=(const uint64_t &other) {
    value *= other;
    return *this;
  }

  constexpr CustomUint64 &operator/=(const uint64_t &other) {
    value /= other;
    return *this;
  }

  constexpr CustomUint64 &operator%=(const uint64_t &other) {
    value %= other;
    return *this;
  }

  // Freundfunktionen für Vergleichsoperatoren
  friend constexpr bool operator==(const uint64_t &lhs,
                                   const CustomUint64 &rhs) {
    return lhs == rhs.value;
  }

  friend constexpr bool operator!=(const uint64_t &lhs,
                                   const CustomUint64 &rhs) {
    return lhs != rhs.value;
  }

  friend constexpr bool operator<(const uint64_t &lhs,
                                  const CustomUint64 &rhs) {
    return lhs < rhs.value;
  }

  friend constexpr bool operator<=(const uint64_t &lhs,
                                   const CustomUint64 &rhs) {
    return lhs <= rhs.value;
  }

  friend constexpr bool operator>(const uint64_t &lhs,
                                  const CustomUint64 &rhs) {
    return lhs > rhs.value;
  }

  friend constexpr bool operator>=(const uint64_t &lhs,
                                   const CustomUint64 &rhs) {
    return lhs >= rhs.value;
  }

  // Freundfunktionen für bitweise Operatoren
  friend constexpr CustomUint64 operator&(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs & rhs.value);
  }

  friend constexpr CustomUint64 operator|(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs | rhs.value);
  }

  friend constexpr CustomUint64 operator^(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs ^ rhs.value);
  }

  // Freundfunktionen für Verschiebeoperatoren
  friend constexpr CustomUint64 operator<<(const uint64_t &lhs,
                                           const CustomUint64 &rhs) {
    return CustomUint64(lhs << rhs.value);
  }

  friend constexpr CustomUint64 operator>>(const uint64_t &lhs,
                                           const CustomUint64 &rhs) {
    return CustomUint64(lhs >> rhs.value);
  }

  // Freundfunktionen für arithmetische Operatoren
  friend constexpr CustomUint64 operator+(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs + rhs.value);
  }

  friend constexpr CustomUint64 operator-(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs - rhs.value);
  }

  friend constexpr CustomUint64 operator*(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs * rhs.value);
  }

  friend constexpr CustomUint64 operator/(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs / rhs.value);
  }

  friend constexpr CustomUint64 operator%(const uint64_t &lhs,
                                          const CustomUint64 &rhs) {
    return CustomUint64(lhs % rhs.value);
  }
};

// Statische Assertion, um sicherzustellen, dass CustomUint64 die gleiche Größe
// wie uint64_t hat
static_assert(sizeof(CustomUint64<false>) == sizeof(uint64_t),
              "CustomUint64 must have the same size as uint64_t");

#endif // CUSTOM_UINT64_HPP