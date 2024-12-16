#include "../DelayedTasksList.hpp"
#include <cstdint>
#pragma once
struct DelayedTasksList;
struct DelayedTasksListPointer {
  std::uintptr_t ptr;

  // Default-Konstruktor: nullptr
  DelayedTasksListPointer() : ptr(0) {}

  // Konstruktor für std::unique_ptr
  template <typename... Args> explicit DelayedTasksListPointer(Args &&...args) {
    auto rawPtr = new DelayedTasksList(std::forward<Args>(args)...);
    ptr = reinterpret_cast<std::uintptr_t>(new DelayedTasksList(std::forward<Args>(args)...));
  }
  // Move-Konstruktor
  DelayedTasksListPointer(DelayedTasksListPointer &&other) noexcept
      : ptr(other.ptr) {
    other.ptr = 0; // Setze das 'andere' Objekt auf nullptr
  }

  DelayedTasksListPointer &operator=(DelayedTasksListPointer &&other) noexcept {
    if (this != &other) {
      reset();         // Bestehende Ressource freigeben
      ptr = other.ptr; // Besitz verschieben
      other.ptr = 0;   // Das 'andere' Objekt auf nullptr setzen
    }
    return *this;
  }
  // Konstruktor für spezielle ungültige Zeiger
  static DelayedTasksListPointer invalid() {
    DelayedTasksListPointer wrapper;
    wrapper.ptr = static_cast<std::uintptr_t>(-1);
    return wrapper;
  }

  bool isValid() const {
    return ptr != 0 && ptr != static_cast<std::uintptr_t>(-1);
  }

  bool isNullptr() const { return ptr == 0; }

  bool isInvalid() const { return ptr == static_cast<std::uintptr_t>(-1); }

  std::unique_ptr<DelayedTasksList> release() {
    if (isValid()) {
      std::unique_ptr<DelayedTasksList> temp(
          reinterpret_cast<DelayedTasksList *>(ptr));
      ptr = 0;
      return temp;
    }
    return nullptr;
  }

  void reset(std::unique_ptr<DelayedTasksList> p = nullptr) {
    if (isValid()) {
      delete reinterpret_cast<DelayedTasksList *>(ptr);
    }
    ptr = reinterpret_cast<std::uintptr_t>(p.release());
  }

  ~DelayedTasksListPointer() {
    if (isValid()) {
      delete reinterpret_cast<DelayedTasksList *>(ptr);
    }
  }
};