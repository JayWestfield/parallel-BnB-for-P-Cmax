#pragma once

#include "_refactoredBnB/Structs/globalValues.hpp"
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>

struct CustomSharedMutex {
  std::vector<std::atomic<int>> referenceCounter;
  std::atomic<bool> clearFlag = false;
  CustomSharedMutex(int maxThreads) : referenceCounter(maxThreads) {
    for (int i = 0; i < maxThreads; ++i) {
      referenceCounter[i].store(0, std::memory_order_relaxed);
    }
  }
  ~CustomSharedMutex() = default;
};

class CustomTrySharedLock {
public:
  inline CustomTrySharedLock(CustomSharedMutex &mtx) : mtx(mtx) {
    if (mtx.clearFlag.load(std::memory_order_acquire))
      return;
    mtx.referenceCounter[ws::thread_index_].store(1, std::memory_order_relaxed);
    if (mtx.clearFlag.load(std::memory_order_acquire)) {
      mtx.referenceCounter[ws::thread_index_].store(0,
                                                    std::memory_order_relaxed);
      return;
    }
    holdsLock = true;
  }
  bool tryLock() {
    if (mtx.clearFlag.load(std::memory_order_acquire))
      return false;
    mtx.referenceCounter[ws::thread_index_].store(1, std::memory_order_release);
    if (mtx.clearFlag.load(std::memory_order_acquire)) {
      mtx.referenceCounter[ws::thread_index_].store(0,
                                                    std::memory_order_release);
      return false;
    }
    holdsLock = true;
    return true;
  }
  inline ~CustomTrySharedLock() {
    if (holdsLock)
      mtx.referenceCounter[ws::thread_index_].store(0,
                                                    std::memory_order_release);
  }
  inline bool owns_lock() { return holdsLock; }

private:
  CustomSharedMutex &mtx;
  bool holdsLock = false;
};
// assert that CustomUniqueLock is not called concurrently (needed because of
// blocking lock befare requiring this lock) otherwise i would need to add a
// lcok wich is good for style / generality but at teh cost of performance (but
// the write is not often yused so i might as well add it)
class CustomUniqueLock {
public:
  inline CustomUniqueLock(CustomSharedMutex &mtx) : mtx(mtx) {
    mtx.clearFlag.store(true, std::memory_order_release);
    bool running = false;
    for (std::size_t i = 0; i < mtx.referenceCounter.size(); i++) {
      running |= static_cast<bool>(
          mtx.referenceCounter[i].load(std::memory_order_acquire));
    }
    while (running) {
      std::this_thread::yield();
      running = false;
      for (std::size_t i = 0; i < mtx.referenceCounter.size(); i++) {
        running |= static_cast<bool>(
            mtx.referenceCounter[i].load(std::memory_order_acquire));
      }
    }
  }
  inline ~CustomUniqueLock() {
    mtx.clearFlag.store(false, std::memory_order_release);
  }

private:
  CustomSharedMutex &mtx;
};