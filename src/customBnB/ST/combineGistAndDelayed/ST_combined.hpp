#ifndef ST_combined_H
#define ST_combined_H

#include "../ST_custom.h"
#include "hashmap/IConcurrentHashMapCombined.h"
#include "hashmap/TBBHashMapCombined.cpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <ostream>
#include <sstream>
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
        suspendedTasks(suspendedTasks), mtx(maxAllowedParallelism) {
    initializeHashMap(HashmapType);
    for (int i = 0; i < maxAllowedParallelism; i++) {
      Gist_storage.push_back(std::make_unique<GistStorage<>>());
    }
    // referenceCounter = 0;
    // clearFlag = false;
  }

  ~ST_combined() {
    CustomUniqueLock lock(mtx);
    resumeAllDelayedTasks();
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

  void computeGist3(const std::vector<int> &state, int job,
                    std::vector<int> &gist) {
    assert(job < jobSize && job >= 0 &&
           std::is_sorted(state.begin(), state.end()));
    assert((state.back() + offset) < maximumRETIndex);
    int maxAllowedOffset = maximumRETIndex;
    // manual unrolling or pragma from open mp? use gist length instead of
    // vecsize ?
    for (size_t i = 0; i < static_cast<size_t>(vec_size); i++) {
      gist[i] = RET[job][state[i] + offset];
      maxAllowedOffset =
          std::min(maxAllowedOffset, findMaxOffset(state[i] + offset, job));
    }
    gist[vec_size] =
        job; // TODO think about wether we really need the jobdepth here (can 2
             // partial assignments with a different depth have teh same gist
             // and if so is that a problem?)
    gist[vec_size + 1] = maxAllowedOffset + offset;
  }
  // TODO that might not be efficient for bigger offsets might use a binary
  // search or try not every but every second entry etc
  inline int findMaxOffset(const int val, const int job) {
    auto entry = RET[job][val];
    auto compare = RET[job][val];
    std::size_t offset = 0;
    // maybe it is better to do it as a oneliner?
    while (entry == compare) {
      compare = RET[job][val + offset++];
    }
    // std::cout << offset - 1 << std::endl;
    return offset - 1;

  }
  inline void resumeTask(DelayedTasksList *next) {
    logging(next->value->state, next->value->job, "runnable");
    suspendedTasks.addTask(next->value);
  }
  inline void resumeTaskList(DelayedTasksList *next) {
    if (next != nullptr) {
      auto current = next;
      // resume delayed tasks
      while (next != reinterpret_cast<DelayedTasksList *>(-1)) {
        resumeTask(next);
        next = next->next;
      }
      if (next != nullptr && next != reinterpret_cast<DelayedTasksList *>(-1))
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
    if (!lock.owns_lock())
      return;
    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return;
    computeGist3(state, job, threadLocalVector);
    auto next = maps->insert(threadLocalVector.data(), true);
    resumeTaskList(next);
    logging(state, job, "added Gist");
  }

  int exists(const std::vector<int> &state, int job) override {
    assert(job < jobSize);
    assert(job >= 0 && job < jobSize);

    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return 0;

    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock())
      return 0;
    if ((state[vec_size - 1] + offset) >= maximumRETIndex || skipThis(job))
      return 0;

    computeGist2(state, job, threadLocalVector);
    const int result = maps->find(threadLocalVector.data());
    return result;
  }

  void addPreviously(const std::vector<int> &state, int job) override {
    assert(job >= 0 && job < jobSize);
    if (skipThis(job))
      return;
    if ((state[vec_size - 1] + offset) >= maximumRETIndex)
      return;
    CustomTrySharedLock lock(mtx);
    if (!lock.owns_lock())
      return;
    if ((state[vec_size - 1] + offset) >= maximumRETIndex)
      return;
    computeGist3(state, job, threadLocalVector);
    maps->insert(threadLocalVector.data(), false);
    logging(state, job, "add Prev");
  }
  auto getReinsertedGistsAndResumeOthers() {
    std::vector<std::pair<std::vector<int>, DelayedTasksList *>>
        gistsToReinsert;

    auto rest = maps->getNonEmptyGists(offset);
    for (auto resume : rest.second) {
      resumeTaskList(resume);
    }
    for (auto list : rest.first) {
      auto job = list.first[gistLength - 1];
      assert(job <= jobSize);
      DelayedTasksList *newList = reinterpret_cast<DelayedTasksList *>(-1);
      DelayedTasksList *iterator = list.second;
      while (iterator != reinterpret_cast<DelayedTasksList *>(-1)) {
        DelayedTasksList *current = iterator;
        iterator = iterator->next;
        // keep it in case the gist is still accurate
        if (current->value->state.back() + offset >= maximumRETIndex) {
          // TODO instantly terminate instead of restarting
          suspendedTasks.addTask(current->value);
          current->next = reinterpret_cast<DelayedTasksList *>(-1);
          delete current;
          continue;
        }
        computeGist2(current->value->state, job, threadLocalVector);
        if (std::equal(list.first, list.first + gistLength,
                       threadLocalVector.data())) {
          current->next = newList;
          newList = current;
        } else {
          resumeTask(current);
          current->next = reinterpret_cast<DelayedTasksList *>(-1);
          delete current;
        }
      }
      std::vector<int> gistWrapped(gistLength + 2);
      for (int i = 0; i < gistLength + 2; ++i) {
        gistWrapped[i] = list.first[i];
      }
      gistsToReinsert.push_back(std::make_pair(gistWrapped, newList));
    }
    return gistsToReinsert;
  }
  void
  reinsertGists(std::vector<std::pair<std::vector<int>, DelayedTasksList *>>
                    gistsToReinsert) {
    for (auto gist : gistsToReinsert) {
      maps->reinsertGist(gist.first.data(), gist.second);
    }
  }
  // assert bound update is itself never called in parallel
  void boundUpdate(int offset) override {
    if (offset <= this->offset)
      return;
    CustomUniqueLock lock(mtx);
    this->offset = offset;
    auto gistsToReinsert = getReinsertedGistsAndResumeOthers();
    // resumeAllDelayedTasks();
    for (std::size_t i = 0; i < Gist_storage.size(); ++i) {
      Gist_storage[i]->clear();
    }
    maps->clear();
    reinsertGists(gistsToReinsert);
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
    if (clearFlag || !lock.owns_lock() ||
        (task->state.back() + offset) >= maximumRETIndex) {
      logging(task->state, task->job, "no no delay");
      suspendedTasks.addTask(task);
      delayedLock.unlock();
      return;
    }
    computeGist3(state, job, threadLocalVector);
    if (!maps->tryAddDelayed(task, threadLocalVector.data())) {
      logging(task->state, task->job, "no no delay");
      suspendedTasks.addTask(task);
    }
  }

private:
  bool detailedLogging = false;
  IConcurrentHashMapCombined *maps = nullptr;
  bool useBitmaps = false; // currently not supported
  std::mutex delayedLock;
  ITaskHolder &suspendedTasks;

  bool clearFlag;
  CustomSharedMutex mtx;

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
    // case 2:
    // maps = new GrowtHashMap();
    // break;
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
  void cancelExecution() override { clearFlag = true; }

  template <typename T>
  void logging(const std::vector<int> &state, int job, T message = "") {
    if (!detailedLogging)
      return;
    std::stringstream gis;
    gis << message << " ";
    for (auto vla : state)
      gis << vla << ", ";
    gis << " => ";
    std::cout << gis.str() << std::endl;
  }
};

#endif
