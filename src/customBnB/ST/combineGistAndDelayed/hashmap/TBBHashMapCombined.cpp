#include "../../../structures/DelayedTasksList.hpp"

#include "IConcurrentHashMapCombined.h"

#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <tbb/concurrent_hash_map.h>
#include <vector>
#pragma once
// TODO the value should only be the delayed lsit not thw whole stEntry
class TBBHashMapCombined : public IConcurrentHashMapCombined {
  using Key = int *;
  using Value = DelayedTasksList *;

  tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher> map_;

public:
  TBBHashMapCombined(std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage)
      : IConcurrentHashMapCombined(Gist_storage){};
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
    } else{
      return false;}
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
  void clear() override { map_.clear(); }
};
