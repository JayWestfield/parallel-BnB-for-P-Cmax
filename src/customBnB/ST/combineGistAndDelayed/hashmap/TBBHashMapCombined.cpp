#include "../../../structures/DelayedTasksList.hpp"
#include "../../../structures/STEntry.hpp"
#include "../../../structures/SegmentedStack.hpp"

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
  using Value = STEntry *;

  tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher> map_;

public:
  TBBHashMapCombined(std::vector<std::unique_ptr<SegmentedStack<>>> &Gist_storage)
      : IConcurrentHashMapCombined(Gist_storage){};
  DelayedTasksList *insert(Key key, bool value) override {
    tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher>::accessor
        accessor;
    auto newEntry = createSTEntry(key, value);
    const bool isNew = map_.insert(accessor, newEntry->gist.data());
    if (isNew) {
      accessor->second = newEntry;
      return value ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
    } else {
      deleteSTEntry();
      if (value) {
        if (accessor->second == nullptr) return nullptr; // TODO that should not be necessary
        const auto delayed = std::move(accessor->second->taskList);
        accessor->second = nullptr;
        return delayed;
      } else {
        return reinterpret_cast<DelayedTasksList *>(-1);
      }
    }
  }

  int find(const Key key) override {
    tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher>::const_accessor
        accessor;
    if (map_.find(accessor, key)) {
      // std::cout << accessor->first << " vs " << key << std::endl;
      return accessor->second == nullptr ? 2 : 1;
    }
    return 0;
  }
  bool tryAddDelayed(std::shared_ptr<CustomTask> &task,
                     int *key) override {
    tbb::concurrent_hash_map<Key, Value, hashingCombined::VectorHasher>::accessor
        accessor;
    if (map_.find(accessor, key)) {
      if (accessor->second->taskList == nullptr)
        return false;
      accessor->second->taskList =
          new DelayedTasksList(task, accessor->second->taskList);
      return true;
    } else
      return false;
  }
  void clear() override { map_.clear(); }
};
