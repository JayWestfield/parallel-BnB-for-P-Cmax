#ifndef ST_combined_H
#define ST_combined_H

#include "../ST_custom.h"
#include "hashmap/IConcurrentHashMapCombined.h"
#include "hashmap/TBBHashMapCombined.cpp"
#include <cassert>
#include <iostream>
#include <memory>
#include <ostream>
#include <sstream>
#include <unistd.h>
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
        suspendedTasks(suspendedTasks) {
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
    for (auto i = 0; i < vec_size; i++) {
      gist[i] = RET[job][state[i] + offset];
    }
    gist[vec_size] =
        job; // TODO think about wether we really need the jobdepth here (can 2
             // partial assignments with a different depth have teh same gist
             // and if so is that a problem?)
  }

  inline void resumeTaskList(DelayedTasksList *next) {
    if (next != nullptr) {
      auto current = next;
      // resume delayed tasks
      while (next != reinterpret_cast<DelayedTasksList *>(-1)) {
        logging(next->value->state, next->value->job, "runnable");
        suspendedTasks.addTask(next->value);
        next = next->next;
        // logging()
        // delete current; TODO unique pointers for simplicity
      }
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
    computeGist2(state, job, threadLocalVector);
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
    computeGist2(state, job, threadLocalVector);
    maps->insert(threadLocalVector.data(), false);
    logging(state, job, "add Prev");
  }

  // assert bound update is itself never called in parallel
  void boundUpdate(int offset) override {
    if (offset <= this->offset)
      return;
    CustomUniqueLock lock(mtx);
    this->offset = offset;
    resumeAllDelayedTasks();

    maps->clear();
  }
  void prepareBoundUpdate() override { mtx.clearFlag = true; }

  void clear() override {
    CustomUniqueLock lock(mtx);
    resumeAllDelayedTasks();
    Gist_storage.clear();
    maps->clear();
  }

  void resumeAllDelayedTasks() override {
    assert(mtx.clearFlag == true);
    for (auto list :     maps->getDelayed()) {
      resumeTaskList(list);
    }
    for (int i = 0; i < Gist_storage.size(); i++) {
      // for (auto entry : *Gist_storage[i]) {
      //   resumeTaskList(entry.taskList);
      // }
      // for (auto it = (*Gist_storage[i]).begin(); it != (*Gist_storage[i]).end();
      //      ++it) {
      //   resumeTaskList((*it).taskList);
      // }
    }
  }

  void addDelayed(std::shared_ptr<CustomTask> task) override {
    auto &state = task->state;
    auto job = task->job;
    assert(job < jobSize && job >= 0);
    CustomTrySharedLock lock(mtx);
    if (clearFlag || !lock.owns_lock() || (task->state.back() + offset) >= maximumRETIndex) {
      logging(task->state, task->job, "no no delay");
      suspendedTasks.addTask(task);
      delayedLock.unlock();
      return;
    }
    computeGist2(state, job, threadLocalVector);
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
