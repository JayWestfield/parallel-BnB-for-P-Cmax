#ifndef ST_combined_H
#define ST_combined_H

#include "../../CustomTaskGroup.hpp"
#include "../ST_custom.h"

#include "customBnB/globalValues.cpp"
#include "customBnB/threadLocal/threadLocal.h"
#include "hashmap/GrowtHashMap.cpp"
#include "hashmap/IConcurrentHashMapCombined.h"
#include "hashmap/TBBHashMapCombined.cpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <ostream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>
// #include "./hashmap/FollyHashMap.cpp"
// #include "./hashmap/JunctionHashMap.cpp"
// #include "./hashmap/GrowtHashMap.cpp"
#include "../../structures/SegmentedStack.hpp"
#include "../../structures/SuspendedTaskHolder.hpp"

#include "../customSharedLock.hpp"
class ST_combined : public ST_custom {
public:
  // using HashMap = tbb::concurrent_hash_map<std::vector<int>, bool,
  // VectorHasher>;

  ST_combined(int jobSize, int offset, const std::vector<std::vector<int>> &RET,
              std::size_t vec_size, ITaskHolder &suspendedTasks,
              int HashmapType, int maxAllowedParallelism)
      : ST_custom(jobSize, offset, RET, vec_size),
        suspendedTasks(suspendedTasks), mtx(maxAllowedParallelism),
        selfIterated(maxAllowedParallelism) {
    for (int i = 0; i < maxAllowedParallelism; i++) {
      Gist_storage.push_back(std::make_unique<GistStorage<>>());
    }
    initializeHashMap(HashmapType);
    // referenceCounter = 0;
    // clearFlag = false;
  }

  ~ST_combined() {
    CustomUniqueLock lock(mtx);
    // resumeAllDelayedTasks(); why should i resume them on deletion?
    if (maps != nullptr)
      delete maps;
  }

  // Deprecated use ComputeGist2 instead
  std::vector<int> computeGist(const std::vector<int> &state,
                               int job) override {
    // assume the state is sorted
    assert(job < jobSize && job >= 0 &&
           std::is_sorted(state.begin(), state.end()));
    std::vector<int> gist(vec_size + 1, 0);
    if ((state.back() + offset) >= maximumRETIndex)
      return {};
    assert((state.back() + offset) <
           maximumRETIndex); // TODO maybe need error Handling to check that
    for (std::vector<int>::size_type i = 0; i < vec_size; i++) {
      gist[i] = RET[job][state[i] + offset];
    }
    gist[vec_size] = job;
    return gist;
  }
  void computeGist2(const std::vector<int> &state, int job,
                    std::vector<int> &gist) override {
    assert(job < jobSize && job >= 0 &&
           std::is_sorted(state.begin(), state.end()));
    assert((state.back() + offset) < maximumRETIndex);
    for (size_t i = 0; i < static_cast<size_t>(vec_size); i++) {
      gist[i] = RET[job][state[i] + offset];
    }
    gist[vec_size] =
        job; // TODO think about wether we really need the jobdepth here (can 2
             // partial assignments with a different depth have teh same gist
             // and if so is that a problem?)
  }
  int findMaxOffset(const int val, const int job, const int currentOffset) {
    const auto entry = RET[job][val + offset];
    std::size_t newMaxOffset = currentOffset;
    auto compare = RET[job][val + offset + newMaxOffset];

    // maybe it is better to do it as a oneliner?
    while (entry != compare) {
      newMaxOffset--;
      compare = RET[job][val + offset + newMaxOffset];
    }
    assert(entry == compare);
    // std::cout << offset - 1 << std::endl;
    assert(newMaxOffset >= 0);
    assert(currentOffset >= newMaxOffset);
    return newMaxOffset;
  }
  void computeGist3(const std::vector<int> &state, int job,
                    std::vector<int> &gist) {
    assert(job < jobSize && job >= 0 &&
           std::is_sorted(state.begin(), state.end()));
    assert((state.back() + offset) < maximumRETIndex);
    int maxAllowedOffset = 0;
    // manual unrolling or pragma from open mp? use gist length instead of
    // vecsize ?
    gist[0] = RET[job][state[0] + offset];
    {
      auto compare = RET[job][state[0] + offset];
      while (RET[job][state[0] + offset + ++maxAllowedOffset] == compare) {
      };
      maxAllowedOffset--;
      assert(maxAllowedOffset >= 0);
    }
    maxAllowedOffset = std::min(
        maxAllowedOffset, maximumRETIndex - state[vec_size - 1] - offset - 1);
    assert(maxAllowedOffset >= 0);
    for (size_t i = 1; i < static_cast<size_t>(vec_size); i++) {
      gist[i] = RET[job][state[i] + offset];
      auto maybenew = findMaxOffset(state[i], job, maxAllowedOffset);
      assert(maxAllowedOffset >= maybenew);
      assert(maybenew >= 0);
      assert(gist[i] == RET[job][state[i] + offset + maybenew]);

      maxAllowedOffset = std::min(maxAllowedOffset, maybenew);
      assert(maxAllowedOffset >= 0);
      assert(gist[i] == RET[job][state[i] + offset + maxAllowedOffset]);
    }
    gist[vec_size] =
        job; // TODO think about wether we really need the jobdepth here (can 2
             // partial assignments with a different depth have teh same gist
             // and if so is that a problem?)
    gist[vec_size + 1] = maxAllowedOffset + offset;

    std::vector<int> gist2(wrappedGistLength);
    for (size_t i = 0; i < static_cast<size_t>(vec_size); i++) {
      gist2[i] = RET[job][state[i] + offset + maxAllowedOffset];
    }
    gist2[vec_size] = job;
    gist2[vec_size + 1] = maxAllowedOffset + offset;
    assert(std::equal(gist.data(), gist.data() + gistLength, gist2.data()));
  }
  // TODO that might not be efficient for bigger offsets might use a binary
  // search
  // TODO also think about the dif between compare and entry can be used as a
  // step width !!!!! search or try not every but every second entry etc

  inline void resumeTask(DelayedTasksList *next) {
    logging(next->value->state, next->value->job, "runnable");
    suspendedTasks.addTask(next->value);
  }
  inline void cancelTask(DelayedTasksList *next) {
    logging(next->value->state, next->value->job, "cancel");
    next->value->parentGroup->unregisterChild();
  }
  inline void cancelTaskList(DelayedTasksList *next) {
    if (next != nullptr) {
      auto current = next;
      // calcel delayed tasks
      while (next != reinterpret_cast<DelayedTasksList *>(-1)) {
        cancelTask(next);
        next = next->next;
      }
      if (current != nullptr &&
          current != reinterpret_cast<DelayedTasksList *>(-1))
        delete current;
    }
  }
  inline void resumeTaskList(DelayedTasksList *next) {
    if (next != nullptr) {
      auto current = next;
      // resume delayed tasks
      while (next != reinterpret_cast<DelayedTasksList *>(-1)) {
        resumeTask(next);
        next = next->next;
      }
      if (current != nullptr &&
          current != reinterpret_cast<DelayedTasksList *>(-1))
        delete current;
    }
  }
  void addGist(const std::vector<int> &state, int job) override {
    assert(job < jobSize && job >= 0);
    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return;

    // referenceCounter++;
    // if (clearFlag)
    // {
    //     referenceCounter--;
    //     return;
    // }
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return;
    computeGist3(state, job, threadLocalVector);
    auto next = maps->insert(threadLocalVector.data(), true);
    // resumeTaskList(next);
    cancelTaskList(next);
    logging(state, job, "added Gist");
  }

  int exists(const std::vector<int> &state, int job) override {
    assert(job < jobSize);
    assert(job >= 0 && job < jobSize);

    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return 0;

    CustomTrySharedLock lock(mtx);
    // TODO maybe the existence check can jsut return 0?
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return 0;

    computeGist2(state, job, threadLocalVector);
    const int result = maps->find(threadLocalVector.data());
    return result;
  }
  void helpWhileLocked(CustomTrySharedLock &lock) {
    while (!lock.tryLock() && !canceled) {
      work();
    }
  }
  void helpWhileLock(std::unique_lock<std::mutex> &lock) override {
    while (!lock.try_lock() && !canceled) {
      work();
    }
  }
  void addPreviously(const std::vector<int> &state, int job) override {
    assert(job >= 0 && job < jobSize);
    if (skipThis(job))
      return;
    if ((state[vec_size - 1] + offset) >= maximumRETIndex)
      return;
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[vec_size - 1] + offset) >= maximumRETIndex)
      return;
    computeGist3(state, job, threadLocalVector);
    maps->insert(threadLocalVector.data(), false);
    logging(state, job, "add Prev");
  }

  void
  reinsertGists(std::vector<std::pair<std::vector<int>, DelayedTasksList *>>
                    gistsToReinsert) {
    for (auto gist : gistsToReinsert) {
      maps->reinsertGist(gist.first.data(), gist.second);
    }
  }
  size_t boundUpdatedCounter = 0;
  // assert bound update is itself never called in parallel
  void boundUpdate(int offset) override {
    if (offset <= this->offset)
      return;
    CustomUniqueLock lock(mtx);
    if (offset <= this->offset)
      return;
    this->offset = offset;
    assert(maybeReinsert.empty());
    assert(restart.empty());
    boundUpdatedCounter++;
    // std::cout << "Bound Update " << boundUpdatedCounter << " offset: " <<
    // offset
    //           << std::endl;
    // TODO check we do not want a copy here
    threadsWorking = maxThreads;
    std::atomic_thread_fence(
        std::memory_order_release); // Synchronisationsbarriere
    assert(threadsWorking.load() == maxThreads);
    // maps->getNonEmptyGists(maybeReinsert2, restart2, offset);
    for (int i = 0; i < maxThreads; i++) {
      selfIterated[i] = 1;
    }
    stepToWork = 10;
    iterateThreadOwnGists();
    selfIterated[threadIndex] = 0;

    // wait for other threads
    int sum = 1;
    while (sum > 0) {
      std::this_thread::yield();
      sum = 0;
      for (int i = 0; i < maxThreads; i++) {
        sum += selfIterated[i];
      }
      if (canceled)
        return;
    }
    /*
        {
          // Sanity check
          std::unordered_map<int *, DelayedTasksList *> maybeReinsertMap,
       maybeReinsert2Map; std::pair<int *, DelayedTasksList *> item; while
       (maybeReinsert.try_pop(item)) { maybeReinsertMap[item.first] =
       item.second;
          }
          while (maybeReinsert2.try_pop(item)) {
            maybeReinsert2Map[item.first] = item.second;
          }
          for (const auto &pair : maybeReinsert2Map) {
            if (maybeReinsertMap.find(pair.first) == maybeReinsertMap.end()) {
              std::cerr << "Sanity check failed: maybeReinsert and
       maybeReinsert2 differ" << std::endl; assert(false);
            }
          }

          std::unordered_map<DelayedTasksList *, int> restartMap, restart2Map;
          DelayedTasksList * item2;
          while (restart.try_pop(item2)) {
            restartMap[item2] = 1;
          }
          while (restart2.try_pop(item2)) {
            restart2Map[item2] = 1;
          }
          for (const auto &pair : restart2Map) {
            if (restartMap.find(pair.first) == restartMap.end()) {
              std::cerr << "Sanity check failed: restart and restart2 differ" <<
       std::endl; assert(false);
            }
          }
              maps->getNonEmptyGists(maybeReinsert, restart, offset);

        }

    */
    stepToWork = 0;

    // resumeAllDelayedTasks();
    while (!maybeReinsert.empty() || !restart.empty()) {
      workOnMaybeReinsert();
      workOnResume();
    }
    // wait for other threads
    while (threadsWorking.load(std::memory_order_acquire) > 0)
      std::this_thread::yield();
    assert(maybeReinsert.empty());
    assert(restart.empty());
    for (std::size_t i = 0; i < Gist_storage.size(); ++i) {
      Gist_storage[i]->clear();
    }
    maps->clear();
    stepToWork.store(1, std::memory_order_relaxed);
    while (!reinsert.empty()) {
      workOnReinsert();
    }
    // wait for other threads (to avoid a race condition with add ing a gist
    // that should resume some tasks that are yet to be reinserted)
    while (threadsWorking > 0)
      std::this_thread::yield();
    stepToWork = 5;
    assert(reinsert.empty());
    clearFlag = false;
  }
  void work() override {
    while (mtx.clearFlag) {
      if (canceled)
        return;

      // idle step
      if (stepToWork.load(std::memory_order_relaxed) == 5) {
        std::this_thread::yield();
        continue;
      }
      if (stepToWork.load(std::memory_order_relaxed) == 10) {
        if (selfIterated[threadIndex] == 1) {
          iterateThreadOwnGists();
          selfIterated[threadIndex] = 0;
        }
      } else if (stepToWork.load(std::memory_order_relaxed) == 0) {
        if (!maybeReinsert.empty()) {
          threadsWorking.fetch_add(1, std::memory_order_relaxed);
          workOnMaybeReinsert();
          threadsWorking.fetch_sub(1, std::memory_order_relaxed);
        }
        if (!restart.empty()) {
          threadsWorking.fetch_add(1, std::memory_order_relaxed);
          workOnResume();
          threadsWorking.fetch_sub(1, std::memory_order_relaxed);
        }
      } else {
        if (!reinsert.empty()) {
          threadsWorking.fetch_add(1, std::memory_order_relaxed);
          workOnReinsert();
          threadsWorking.fetch_sub(1, std::memory_order_relaxed);
        }
      }
    }
  }
  void prepareBoundUpdate() override { mtx.clearFlag = true; }

  void clear() override {
    CustomUniqueLock lock(mtx);
    resumeAllDelayedTasks();
    // Gist_storage.clear();
    for (std::size_t i = 0; i < Gist_storage.size(); ++i) {
      Gist_storage[i]->clear();
    }
    maps->clear();
  }

  void resumeAllDelayedTasks() override {
    assert(mtx.clearFlag == true);
    for (auto list : maps->getDelayed()) {
      resumeTaskList(list);
    }
  }

  void addDelayed(std::shared_ptr<CustomTask> task) override {
    auto &state = task->state;
    auto job = task->job;
    assert(job < jobSize && job >= 0);
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if (clearFlag || (task->state.back() + offset) >= maximumRETIndex) {
      // TODO on second case cancle task
      logging(task->state, task->job, "no no delay1");
      suspendedTasks.addTask(task);
      delayedLock.unlock();
      return;
    }
    computeGist3(state, job, threadLocalVector);
    if (!maps->tryAddDelayed(task, threadLocalVector.data())) {
      logging(task->state, task->job, "no no delay2");
      suspendedTasks.addTask(task);
    }
  }
  void workOnMaybeReinsert() {
    if (maybeReinsert.empty())
      return;

    std::pair<int *, DelayedTasksList *> work;
    while (maybeReinsert.try_pop(work)) {
      const auto job = work.first[gistLength - 1];
      assert(job <= jobSize);
      DelayedTasksList *newList = reinterpret_cast<DelayedTasksList *>(-1);
      DelayedTasksList *iterator = work.second;
      std::vector<int> gistWrapped(wrappedGistLength);
      // TODO use memcopy / initialize with values
      for (int i = 0; i < wrappedGistLength; ++i) {
        gistWrapped[i] = work.first[i];
      }
      while (iterator != reinterpret_cast<DelayedTasksList *>(-1)) {
        DelayedTasksList *current = iterator;
        iterator = iterator->next;
        // keep it in case the gist is still accurate
        if (current->value->state.back() + offset >= maximumRETIndex) {
          // TODO instantly terminate instead of restarting
          cancelTask(current);
          current->next = reinterpret_cast<DelayedTasksList *>(-1);
          delete current;
          continue;
        }
        computeGist2(current->value->state, job, threadLocalVector);
        // equal could be shorter by one last one is the job
        if (std::equal(work.first, work.first + gistLength,
                       threadLocalVector.data())) {
          current->next = newList;
          newList = current;
        } else {
          logging(current->value->state, current->value->job,
                  "WorkonMaybeReinsert");
          resumeTask(current);
          current->next = reinterpret_cast<DelayedTasksList *>(-1);
          delete current;
        }
      }

      reinsert.push(std::make_pair(gistWrapped, newList));
    }
  }
  void workOnResume() {
    if (restart.empty())
      return;
    DelayedTasksList *work;
    while (restart.try_pop(work)) {
      logging(work->value->state, work->value->job, "workOnResume");
      resumeTaskList(work);
    }
  }
  void workOnReinsert() {
    if (reinsert.empty())
      return;
    std::pair<std::vector<int>, DelayedTasksList *> work;
    while (reinsert.try_pop(work)) {
      // resumeTaskList(work.second);
      maps->reinsertGist(work.first.data(), work.second);
    }
  }

  void iterateThreadOwnGists() {
    maps->iterateThreadOwnGists(offset, maybeReinsert, restart);
    threadsWorking.fetch_sub(1);
  }

private:
  bool detailedLogging = false;
  IConcurrentHashMapCombined *maps = nullptr;
  bool useBitmaps = false; // currently not supported
  std::mutex delayedLock;
  ITaskHolder &suspendedTasks;
  std::atomic<bool> clearFlag = false;
  CustomSharedMutex mtx;
  std::atomic<bool> canceled = false;
  // migration
  std::atomic<int> threadsWorking = 0;
  std::atomic<int> stepToWork = 5;

  tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>> maybeReinsert;
  tbb::concurrent_queue<DelayedTasksList *> restart;
  tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>> maybeReinsert2;
  tbb::concurrent_queue<DelayedTasksList *> restart2;
  tbb::concurrent_queue<std::pair<std::vector<int>, DelayedTasksList *>>
      reinsert;
  std::vector<std::atomic<int>> selfIterated;
  // ST
  std::vector<std::unique_ptr<GistStorage<>>> Gist_storage;

  void initializeHashMap(int type) {
    if (maps != nullptr)
      delete maps;
    switch (type) {
    case 0:
      maps = new TBBHashMapCombined(Gist_storage);
      break;
    // case 1:
    //     maps = new FollyHashMap();
    //     break;
    case 2:
      maps = new GrowtHashMap(Gist_storage);
      break;
    // case 3 :
    //     maps = new JunctionHashMap();
    //     break;
    default:
      maps = new TBBHashMapCombined(Gist_storage);
    }
  }

  // experimental to not add every depth to the ST
  inline bool skipThis(int depth) {
    return false; // depth % 2 == 0;
  }
  void cancelExecution() override { clearFlag = true; canceled = true; }

  template <typename T>
  void logging(const std::vector<int> &state, int job, T message = "") {
    if (!detailedLogging)
      return;
    std::stringstream gis;
    gis << message << " ";
    for (auto vla : state)
      gis << vla << ", ";
    // gis << " => "  << std::endl;
    gis << " Job: " << job << std::endl;

    std::cout << gis.str();
  }
};

#endif
