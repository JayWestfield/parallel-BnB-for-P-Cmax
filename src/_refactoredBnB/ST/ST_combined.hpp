#pragma once

#include "../Structs/DelayedTasksList.hpp"
#include "../Structs/TaskContext.hpp"

#include "./FingerPrintUtil.hpp"
#include "_refactoredBnB/Structs/GistStorage.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include "customSharedLock.hpp"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <mutex>
#include <oneapi/tbb/concurrent_queue.h>
#include <ostream>
#include <sstream>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>
template <typename HashTable, bool use_fingerprint,
          bool detailedLogging = false>
class ST {

public:
  ST(int offset, const std::vector<std::vector<int>> &RET,
     wss<TaskContext> &scheduler, int mapInitialSize, int maxThreads,
     int GistStorageSize)
      : offset(offset), RET(RET), maps(mapInitialSize, maxThreads),
        scheduler(scheduler), mtx(maxThreads), maxThreads(maxThreads),
        selfIterated(maxThreads) {
    for (int i = 0; i < maxThreads; i++) {
      Gist_storage.push_back(GistStorage(GistStorageSize));
    }
  }

  ~ST() { CustomUniqueLock lock(mtx); }

  void computeGist(const std::vector<int> &state, int job,
                   std::vector<int> &gist) {
    assert(std::is_sorted(state.begin(), state.end()));
    assert((state.back() + offset) < RET[0].size());
    const auto &retRow = RET[job];
#pragma omp unroll 8
    for (size_t i = 0; i < ws::stateLength; i++) {
      gist[i] = retRow[state[i] + offset];
    }
    gist[ws::stateLength] = job;
  }
  int findMaxOffset(const int val, const int job, const int currentOffset) {
    const auto entry = RET[job][val + offset];
    std::size_t newMaxOffset = currentOffset;
    auto compare = RET[job][val + offset + newMaxOffset];

    // TODO test which one is better
    while (entry != compare) {
      // newMaxOffset -= entry - compare;
      newMaxOffset--;
      compare = RET[job][val + offset + newMaxOffset];
    }
    assert(entry == compare);
    assert(newMaxOffset >= 0);

    assert(currentOffset >= newMaxOffset);
    return newMaxOffset;
  }

  void computeGistWithMaxOffset(const std::vector<int> &state, int job,
                                std::vector<int> &gist) {
    assert(std::is_sorted(state.begin(), state.end()));
    assert((state.back() + offset) < RET[0].size());
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
    maxAllowedOffset =
        std::min(maxAllowedOffset,
                 int(RET[0].size() - state[ws::stateLength - 1] - offset - 1));
    assert(maxAllowedOffset >= 0);
    for (size_t i = 1; i < static_cast<size_t>(ws::stateLength); i++) {
      gist[i] = RET[job][state[i] + offset];
      auto maybenew = findMaxOffset(state[i], job, maxAllowedOffset);
      assert(maxAllowedOffset >= maybenew);
      assert(maybenew >= 0);
      assert(gist[i] == RET[job][state[i] + offset + maybenew]);

      maxAllowedOffset = std::min(maxAllowedOffset, maybenew);
      assert(maxAllowedOffset >= 0);
      assert(gist[i] == RET[job][state[i] + offset + maxAllowedOffset]);
    }
    gist[ws::stateLength] =
        job; // TODO think about wether we really need the jobdepth here (can 2
             // partial assignments with a different depth have teh same gist
             // and if so is that a problem?)
    gist[ws::gistLength] = maxAllowedOffset + offset;

    /// i think gist2 was just a sanity chek => remove it
    std::vector<int> gist2(ws::wrappedGistLength);
    for (size_t i = 0; i < static_cast<size_t>(ws::stateLength); i++) {
      gist2[i] = RET[job][state[i] + offset + maxAllowedOffset];
    }
    gist2[ws::stateLength] = job;
    gist2[ws::gistLength] = maxAllowedOffset + offset;
    assert(std::equal(gist.data(), gist.data() + ws::gistLength, gist2.data()));
  }
  // TODO that might not be efficient for bigger offsets might use a binary
  // search
  // TODO also think about the dif between compare and entry can be used as a
  // step width !!!!! search or try not every but every second entry etc

  void addGist(const std::vector<int> &state, int job) {
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size() || skipThis(job))
      return;

    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size())
      return;
    computeGistWithMaxOffset(state, job, ws::threadLocalVector);

    auto next = maps.addGist(FingerPrintUtil<use_fingerprint>::addFingerprint(
        createGistEntry(ws::threadLocalVector.data())));
    // signals wether to keep the gist or not
    if (!next.first) {
      deleteGistEntry();
    }
    if (next.second != nullptr) {
      cancelTaskList(next.second);
    }
    logging(ws::threadLocalVector, job, "added Gist");
  }

  FindGistResult exists(const std::vector<int> &state, int job) {
    // TODO it was -1 but the gist contains the job so to acces the last stat it
    // would be gist - 2 riught
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size() || skipThis(job))
      return FindGistResult::NOT_FOUND;

    CustomTrySharedLock lock(mtx);
    // TODO maybe the existence check can jsut return 0?
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size() || skipThis(job))
      return FindGistResult::NOT_FOUND;

    computeGist(state, job, ws::threadLocalVector);
    const FindGistResult result =
        maps.find(FingerPrintUtil<use_fingerprint>::addFingerprint(
            ws::threadLocalVector.data()));
    return result;
  }
  void helpWhileLocked(CustomTrySharedLock &lock) {
    while (!lock.tryLock() && !canceled) {
      work();
    }
  }
  void helpWhileLock(std::unique_lock<std::mutex> &lock) {
    while (!lock.try_lock() && !canceled) {
      work();
    }
  }
  // TODO if teh insert is not succesfull the entry is either finihsed or
  // already adde previously => either suspend or cancel The task
  void addPreviously(const std::vector<int> &state, int job) {
    if (skipThis(job))
      return;
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size())
      return;
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
    }
    if ((state[ws::gistLength - 2] + offset) >= RET[0].size())
      return;
    computeGistWithMaxOffset(state, job, ws::threadLocalVector);
    bool succesfullInsert =
        maps.addPreviously(FingerPrintUtil<use_fingerprint>::addFingerprint(
            createGistEntry(ws::threadLocalVector.data())));
    if (!succesfullInsert)
      deleteGistEntry();

    logging(ws::threadLocalVector, job, "add Prev");
  }

  void
  reinsertGists(std::vector<std::pair<std::vector<int>, DelayedTasksList *>>
                    gistsToReinsert) {
    for (auto gist : gistsToReinsert) {
      maps.reinsertGist(FingerPrintUtil<use_fingerprint>::addFingerprint(
                            createGistEntry(Gist_storage, gist.first.data())),
                        gist.second);
      // maps.reinsertGist(gist.first.data(), gist.second);
    }
  }
  size_t boundUpdatedCounter = 0;
  // assert bound update is itself never called in parallel
  void boundUpdate(int offset) {
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
    // std::atomic_thread_fence(
    //     std::memory_order_release); // Synchronisationsbarriere
    assert(threadsWorking.load() == maxThreads);
    maps.getNonEmptyGists(maybeReinsert2, restart2, offset);
    for (int i = 0; i < maxThreads; i++) {
      selfIterated[i] = 1;
    }
    stepToWork = 10;
    // std::cout << "step to Work: " << stepToWork << std::endl;
    iterateThreadOwnGists();
    selfIterated[ws::thread_index_] = 0;
    // maps.getNonEmptyGists(maybeReinsert, restart, offset);
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

    stepToWork = 0;
    // std::cout << "step to Work: " << stepToWork << std::endl;

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
    maps.BoundUpdateClear();

    for (std::size_t i = 0; i < Gist_storage.size(); ++i) {
      Gist_storage[i].clear();
    }
    stepToWork.store(1, std::memory_order_relaxed);
    // std::cout << "step to Work: " << stepToWork << std::endl;

    while (!reinsert.empty()) {
      workOnReinsert();
    }
    // wait for other threads (to avoid a race condition with add ing a gist
    // that should resume some tasks that are yet to be reinserted)
    while (threadsWorking > 0)
      std::this_thread::yield();
    stepToWork = 5;
    // std::cout << "step to Work: " << stepToWork << std::endl;

    assert(reinsert.empty());
    clearFlag = false;
  }
  // TODO Eviction policy and execution !!!!
  void evict() { clear(); }
  void work() {
    while (mtx.clearFlag) {
      if (canceled)
        return;

      // idle step
      if (stepToWork.load(std::memory_order_relaxed) == 5) {
        std::this_thread::yield();
        continue;
      }
      if (stepToWork.load(std::memory_order_relaxed) == 10) {
        if (selfIterated[ws::thread_index_] == 1) {
          iterateThreadOwnGists();
          selfIterated[ws::thread_index_] = 0;
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
        std::this_thread::yield();
      } else {
        if (!reinsert.empty()) {
          threadsWorking.fetch_add(1, std::memory_order_relaxed);
          workOnReinsert();
          threadsWorking.fetch_sub(1, std::memory_order_relaxed);
        }
        std::this_thread::yield();
      }
    }
  }
  void prepareBoundUpdate() { mtx.clearFlag = true; }

  void clear() {
    CustomUniqueLock lock(mtx);
    resumeAllDelayedTasks();
    // Gist_storage.clear();
    for (std::size_t i = 0; i < Gist_storage.size(); ++i) {
      Gist_storage[i].clear();
    }
    maps.clear();
  }

  void resumeAllDelayedTasks() {
    assert(mtx.clearFlag == true);
    for (auto list : maps.getDelayed()) {
      resumeTaskList(list);
    }
  }

  void addDelayed(Task<TaskContext> *task) {
    auto &state = task->context.state;
    auto job = task->context.job;
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock()) {
      helpWhileLocked(lock);
      scheduler.resumeTask(task);
      return;
    }

    if (clearFlag || (task->context.state.back() + offset) >= RET[0].size()) {
      // TODO on second case cancle task
      logging(task->context.state, task->context.job, "no no delay1");
      scheduler.resumeTask(task);
      return;
    }

    computeGistWithMaxOffset(state, job, ws::threadLocalVector);
    // computeGist(state, job, ws::threadLocalVector);
    logging(ws::threadLocalVector, task->context.job, "AddDelay");
    logging(state, task->context.job, "AddDelay");

    bool delayed = maps.tryAddDelayed(
        task, FingerPrintUtil<use_fingerprint>::addFingerprint(
                  createGistEntry(ws::threadLocalVector.data())));
    // TODO the key might have an issue if we use fingerprints

    deleteGistEntry();

    if (!delayed) {
      logging(task->context.state, task->context.job, "no no delay2");
      scheduler.resumeTask(task);
    }
  }
  void workOnMaybeReinsert() {
    if (maybeReinsert.empty())
      return;

    std::pair<int *, DelayedTasksList *> work;
    while (maybeReinsert.try_pop(work)) {
      const auto job = work.first[ws::gistLength - 1];
      DelayedTasksList *newList = reinterpret_cast<DelayedTasksList *>(-1);
      DelayedTasksList *iterator = work.second;
      std::vector<int> gistWrapped(ws::wrappedGistLength);
      // TODO use memcopy / initialize with values
      for (int i = 0; i < ws::wrappedGistLength; ++i) {
        gistWrapped[i] = work.first[i];
      }
      while (iterator != reinterpret_cast<DelayedTasksList *>(-1)) {
        DelayedTasksList *current = iterator;
        iterator = iterator->next;
        // keep it in case the gist is still accurate
        if (current->value->context.state.back() + offset >= RET[0].size()) {
          // TODO instantly terminate instead of restarting
          cancelTask(current);
          current->next = reinterpret_cast<DelayedTasksList *>(-1);
          delete current;
          continue;
        }
        computeGist(current->value->context.state, job, ws::threadLocalVector);
        // equal could be shorter by one last one is the job
        if (std::equal(work.first, work.first + ws::gistLength,
                       ws::threadLocalVector.data())) {
          logging(current->value->context.state, current->value->context.job,
                  "reinsert");
          current->next = newList;
          newList = current;
        } else {
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
      logging(work->value->context.state, work->value->context.job,
              "workOnResume");
      resumeTaskList(work);
    }
  }
  void workOnReinsert() {
    if (reinsert.empty())
      return;
    std::pair<std::vector<int>, DelayedTasksList *> work;
    while (reinsert.try_pop(work)) {
      // resumeTaskList(work.second);
      maps.reinsertGist(FingerPrintUtil<use_fingerprint>::addFingerprint(
                            createGistEntry((work.first.data()))),
                        work.second);
    }
  }
  // one could access maybeReinsert etc in batches but currently it is no
  // bottleneck
  void iterateThreadOwnGists() {
    maps.iterateThreadOwnGists(offset, Gist_storage[ws::thread_index_],
                               maybeReinsert, restart);
    threadsWorking.fetch_sub(1);
  }
  void cancelExecution() {
    clearFlag = true;
    canceled = true;
  }

private:
  int maxThreads;
  int offset; // no atomic because the bound Update is sequential
  const std::vector<std::vector<int>> &RET;

  wss<TaskContext> &scheduler;
  bool useBitmaps = false; // currently not supported
  std::atomic<bool> clearFlag = false;
  CustomSharedMutex mtx;
  std::atomic<bool> canceled = false;
  // migration
  std::atomic<int> threadsWorking = 0;
  std::atomic<int> stepToWork = 5;
  HashTable maps;
  tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>> maybeReinsert;
  tbb::concurrent_queue<DelayedTasksList *> restart;
  tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>> maybeReinsert2;
  tbb::concurrent_queue<DelayedTasksList *> restart2;
  tbb::concurrent_queue<std::pair<std::vector<int>, DelayedTasksList *>>
      reinsert;
  std::vector<std::atomic<int>> selfIterated;
  // ST
  std::vector<GistStorage> Gist_storage;

  // experimental to not add every depth to the ST
  inline bool skipThis(int depth) {
    return false; // depth % 2 == 0;
  }
  int *createGistEntry(int *gist) {
    return Gist_storage[ws::thread_index_].push(gist);
  }
  void deleteGistEntry() { Gist_storage[ws::thread_index_].pop(); }
  inline void resumeTask(DelayedTasksList *next) {
    logging(next->value->context.state, next->value->context.job, "runnable");
    scheduler.resumeTask(next->value);
  }
  inline void cancelTask(DelayedTasksList *next) {
    logging(next->value->context.state, next->value->context.job, "cancel");
    scheduler.cancelTask(next->value);
  }
  inline void cancelTaskList(DelayedTasksList *next) {
    assert(next != nullptr);
    if (next != nullptr) {

      auto current = next;
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
      while (next != reinterpret_cast<DelayedTasksList *>(-1)) {
        resumeTask(next);
        next = next->next;
      }
      if (current != reinterpret_cast<DelayedTasksList *>(-1))
        delete current;
    }
  }
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
