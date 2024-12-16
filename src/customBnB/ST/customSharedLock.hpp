#include <atomic>
#include <thread>
#pragma once
struct CustomSharedMutex {
  std::atomic<int> referenceCounter = 0;
  std::atomic<bool> clearFlag = false;
};

class CustomTrySharedLock {
public:
  inline CustomTrySharedLock(CustomSharedMutex &mtx) : mtx(mtx) {
    if (mtx.clearFlag.load(std::memory_order_relaxed))
      return;
    mtx.referenceCounter.fetch_add(1, std::memory_order_relaxed);
    if (mtx.clearFlag.load(std::memory_order_relaxed)) {
      mtx.referenceCounter.fetch_sub(1, std::memory_order_relaxed);
      return;
    }
    holdsLock = true;
  }
  inline ~CustomTrySharedLock() {
    if (holdsLock)
      mtx.referenceCounter.fetch_sub(1, std::memory_order_relaxed);
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
    while (mtx.referenceCounter.load(std::memory_order_relaxed) > 0) {
      std::this_thread::yield();
    }
  }
  inline ~CustomUniqueLock() { mtx.clearFlag = false; }

private:
  CustomSharedMutex &mtx;
};