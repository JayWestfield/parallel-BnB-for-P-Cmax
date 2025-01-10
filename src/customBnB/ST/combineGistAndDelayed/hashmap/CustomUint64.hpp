#ifndef CUSTOM_UINT64_HPP
#define CUSTOM_UINT64_HPP

#include "../../../globalValues.cpp"
#include <algorithm>
#include <cstdint>
#include <type_traits>
static constexpr size_t marked_bit = 1ull << 63;
static constexpr size_t bitmask = marked_bit - 1;
static constexpr size_t max = 0xffffffffffffffff;

struct CustomUint64 {
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
    else
    return std::equal(reinterpret_cast<Key>(value & bitmask),
                      reinterpret_cast<Key>(value & bitmask) + gistLength,
                      reinterpret_cast<Key>(other.value & bitmask));

    // return this->value == other.value;
  }

  constexpr bool operator==(const uint64_t &other) {
    if (other == 0)
      return this->value == other;
    else
      return std::equal(reinterpret_cast<Key>(value),
                        reinterpret_cast<Key>(value) + gistLength,
                        reinterpret_cast<Key>(other));
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
  constexpr CustomUint64 operator&(const uint64_t &other) const {
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
static_assert(sizeof(CustomUint64) == sizeof(uint64_t),
              "CustomUint64 must have the same size as uint64_t");

#endif // CUSTOM_UINT64_HPP