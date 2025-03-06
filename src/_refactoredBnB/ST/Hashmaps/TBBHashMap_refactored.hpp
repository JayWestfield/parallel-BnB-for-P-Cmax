#pragma once

#include "_refactoredBnB/ST/Hashmaps/hashing/hashing.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/TaskContext.hpp"
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
template <bool use_fingerprint> class TBBHashMap_refactored {
  using Key = int *;
  using Value = DelayedTasksList *;

  tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher> map_;

public:
  TBBHashMap_refactored(int initialHashmapSize, int maxAllowedParallelism)
      : map_(initialHashmapSize){};

  DelayedTasksList *insert(Key key, bool value) {
    assert(false && "unused/deprecated");
    // tbb::concurrent_hash_map<Key, Value,
    //                          hashingCombined::VectorHasher>::accessor
    //                          accessor;
    // auto newEntry = createGistEntry(key);

    // const bool isNew = map_.insert(accessor, key);
    // if (isNew) {
    //   accessor->second =
    //       value ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
    //   return value ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
    // } else {
    //   deleteGistEntry();
    //   if (value) {
    //     if (accessor->second == nullptr)
    //       return nullptr; // TODO that should not be necessary
    //     const auto delayed = std::move(accessor->second);
    //     accessor->second = nullptr;
    //     return delayed;
    //   } else {
    //     return reinterpret_cast<DelayedTasksList *>(-1);
    //   }
    // }
  }
  std::pair<bool, DelayedTasksList *> addGist(Key key) {
    // std::cout << key << std::endl;

    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;

    const bool isNew = map_.insert(accessor, key);
    const auto delayed = isNew ? nullptr : accessor->second;
    accessor->second = nullptr;
    return std::make_pair(isNew, delayed);
  }

  bool addPreviously(Key key) {
    // std::cout << key << std::endl;

    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    const bool isNew = map_.insert(accessor, key);
    if (isNew) {
      accessor->second = reinterpret_cast<DelayedTasksList *>(-1);
      return true;
    }
    return false;
  }
  bool tryAddDelayed(Task<TaskContext> *task, int *key) {
    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    if (map_.find(accessor, key)) {
      if (accessor->second == nullptr) {
        return false;
      }
      accessor->second = new DelayedTasksList(task, accessor->second);
      return true;
    } else {
      return false;
    }
  }
  void reinsertGist(int *gist, DelayedTasksList *delayed) {
    tbb::concurrent_hash_map<Key, Value,
                             hashingCombined::VectorHasher>::accessor accessor;
    // std::cout << "reinsert " << gist << std::endl;

    const bool isNew = map_.insert(accessor, gist);

    assert(isNew);
    accessor->second = delayed;
  }

  FindGistResult find(const Key key) {
    tbb::concurrent_hash_map<
        Key, Value, hashingCombined::VectorHasher>::const_accessor accessor;
    if (map_.find(accessor, key)) {
      return accessor->second == nullptr ? FindGistResult::COMPLETED
                                         : FindGistResult::STARTED;
    }
    return FindGistResult::NOT_FOUND;
  }

  std::vector<DelayedTasksList *> getDelayed() {
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
                   const int newOffset) {
    std::pair<tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>,
              tbb::concurrent_queue<DelayedTasksList *>>
        nonEmpty;
    for (auto entry : map_) {
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<DelayedTasksList *>(-1)) {
        // check the max offset TODO check is gist length correct ?
        if (entry.first[ws::gistLength] >= newOffset) {
          maybeReinsert.push(entry);
        } else {
          restart.push(entry.second);
        }
      }
    }
  }

  void iterateThreadOwnGists(
      int offset, GistStorage storageToIterate,
      tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
          &maybeReinsert,
      tbb::concurrent_queue<DelayedTasksList *> &restart) {
    // return;
    // std::cout << "iterate " << threadIndex << " offset: " << offset <<
    // std::endl; std::cout << "size " << map_.size() << std::endl;
    for (auto it : storageToIterate) {
      tbb::concurrent_hash_map<
          Key, Value, hashingCombined::VectorHasher>::const_accessor accessor;
      auto value = map_.find(accessor, it);
      assert(value);
      if (accessor->second != nullptr &&
          accessor->second != reinterpret_cast<DelayedTasksList *>(-1)) {
        if (accessor->first[ws::gistLength] >= offset) {
          maybeReinsert.push(*accessor);
        } else {
          restart.push(accessor->second);
        }
      }
    }
    // std::cout << "elements " << counter << std::endl;
  }
  void BoundUpdateClear() { map_.clear(); }

  void clear() { map_.clear(); }
};
