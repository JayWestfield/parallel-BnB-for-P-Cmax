#include "../threadLocal/threadLocal.h"
#include <atomic>
#include <cstddef>
#include <thread>
#include <vector>
#pragma once
struct CustomSharedMutex {
  std::vector<std::atomic<int>> referenceCounter;
  std::atomic<bool> clearFlag = false;
  CustomSharedMutex(int maxThreads): referenceCounter(maxThreads) {
    for (int i = 0; i < maxThreads; ++i) {
      referenceCounter[i].store(0, std::memory_order_relaxed);
    }
  }
};

class CustomTrySharedLock {
public:
  inline CustomTrySharedLock(CustomSharedMutex &mtx) : mtx(mtx) {
    if (mtx.clearFlag.load(std::memory_order_relaxed))
      return;
    mtx.referenceCounter[threadIndex].store(1, std::memory_order_relaxed);
    if (mtx.clearFlag.load(std::memory_order_relaxed)) {
      mtx.referenceCounter[threadIndex].store(0, std::memory_order_relaxed);
      return;
    }
    holdsLock = true;
  }
  inline ~CustomTrySharedLock() {
    if (holdsLock)
      mtx.referenceCounter[threadIndex].store(0, std::memory_order_relaxed);
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
    mtx.clearFlag = true;
    bool running = false;
    for (std::size_t i = 0; i < mtx.referenceCounter.size(); i++) {
      running |= static_cast<bool>(
          mtx.referenceCounter[i].load(std::memory_order_relaxed));
    }
    while (running) {
      std::this_thread::yield();
      running = false;
      for (std::size_t i = 0; i < mtx.referenceCounter.size(); i++) {
        running |= static_cast<bool>(
            mtx.referenceCounter[i].load(std::memory_order_relaxed));
      }
    }
  }
  inline ~CustomUniqueLock() { mtx.clearFlag = false; }

private:
  CustomSharedMutex &mtx;
};