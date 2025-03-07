#pragma once

#include "../../../structures/DelayedTasksList.hpp"

#include "IConcurrentHashMapCombined.h"
#include "customBnB/threadLocal/threadLocal.h"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <tbb/concurrent_hash_map.h>
#include <utility>
#include <vector>
// TODO the value should only be the delayed lsit not thw whole stEntry
class TBBHashMapCombined : public IConcurrentHashMapCombined {
  using Key = int *;
  using Value = DelayedTasksList *;

  tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher> map_;

public:
  TBBHashMapCombined(std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage)
      : IConcurrentHashMapCombined(Gist_storage), map_(5000000000){};
  DelayedTasksList *insert(Key key, bool value) override {
    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    auto newEntry = createGistEntry(key);

    const bool isNew = map_.insert(accessor, newEntry);
    if (isNew) {
      accessor->second =
          value ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
      return value ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
    } else {
      deleteGistEntry();
      if (value) {
        if (accessor->second == nullptr)
          return nullptr; // TODO that should not be necessary
        const auto delayed = std::move(accessor->second);
        accessor->second = nullptr;
        return delayed;
      } else {
        return reinterpret_cast<DelayedTasksList *>(-1);
      }
    }
  }
  void reinsertGist(int *gist, DelayedTasksList *delayed) override {
    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    auto newEntry = createGistEntry(gist);

    const bool isNew = map_.insert(accessor, newEntry);
    assert(isNew);
    accessor->second = delayed;
  }

  int find(const Key key) override {
    tbb::concurrent_hash_map<
        Key, Value, hashingCombined::VectorHasher>::const_accessor accessor;
    if (map_.find(accessor, key)) {
      return accessor->second == nullptr ? 2 : 1;
    }
    return 0;
  }
  bool tryAddDelayed(std::shared_ptr<CustomTask> &task, int *key) override {
    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    if (map_.find(accessor, key)) {

      // check for nullptr
      if (accessor->second == nullptr) {
        return false;
      }
      accessor->second = new DelayedTasksList(task, accessor->second);
      return true;
    } else {
      return false;
    }
  }
  std::vector<DelayedTasksList *> getDelayed() override {
    std::vector<DelayedTasksList *> delayedLists;
    for (auto entry : map_) {
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<DelayedTasksList *>(-1)) {
        delayedLists.push_back(entry.second);
      }
    }
    return delayedLists;
  }
  // cannot filter the gist here, because we do not have access to the compute
  // gist necessary to chekc wether the delayed tast
  // first describes entries that might be reinserted
  void
  getNonEmptyGists(tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
                       &maybeReinsert,
                   tbb::concurrent_queue<DelayedTasksList *> &restart,
                   const int newOffset) override {
    std::pair<tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>,
              tbb::concurrent_queue<DelayedTasksList *>>
        nonEmpty;
    for (auto entry : map_) {
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<DelayedTasksList *>(-1)) {
        // check the max offset TODO check is gist length correct ?
        if (entry.first[gistLength] >= newOffset) {
          maybeReinsert.push(entry);
        } else {
          restart.push(entry.second);
        }
      }
    }
  }

  void iterateThreadOwnGists(
      int offset,
      tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
          &maybeReinsert,
      tbb::concurrent_queue<DelayedTasksList *> &restart) override {
    // std::cout << "iterate " << threadIndex << " offset: " << offset <<
    // std::endl; std::cout << "size " << map_.size() << std::endl;
    int counter = 0;
    for (auto it = (*Gist_storage[threadIndex]).begin();
         it != (*Gist_storage[threadIndex]).end(); ++it) {
      counter++;
      int *gist = *it;
      tbb::concurrent_hash_map<
          Key, Value, hashingCombined::VectorHasher>::const_accessor accessor;
      auto value = map_.find(accessor, gist);
      assert(value);
      if (accessor->second != nullptr &&
          accessor->second != reinterpret_cast<DelayedTasksList *>(-1)) {
        if (accessor->first[gistLength] >= offset) {
          maybeReinsert.push(*accessor);
        } else {
          restart.push(accessor->second);
        }
      }
    }
    // std::cout << "elements " << counter << std::endl;
  }

  void clear() override { map_.clear(); }
};
