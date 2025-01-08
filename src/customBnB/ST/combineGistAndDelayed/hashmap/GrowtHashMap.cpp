#pragma once
#include "../../../structures/DelayedTasksList.hpp"
#include "IConcurrentHashMapCombined.h"
// #include <growt/alignedallocator.hpp>
#include "allocator/alignedallocator.hpp"
#include "data-structures/table_config.hpp"

// #include "hash_table_mods.hpp"

#include <iostream>
#include <string>
// #include <table_config.h>
// #include <growt>

#include <utility>
#include <vector>

// TODO update to use the DelayedTasksList
// TODO check muss ich das value ding auch als reinterpret cast machen? weil ich
// gklaube das wird als pointer gewrapped
class GrowtHashMap : public IConcurrentHashMapCombined {
  using Key = int *;
  using StoreKey = uint64_t;
  using Value = DelayedTasksList *;
  // evtl geht das acuh oghne das reinterpret cast (für value) glaube das
  // problme war das fehlende atomic
  using StoreValue = Value;//uint64_t;

  using TaskPointer = std::shared_ptr<CustomTask>;
  using allocator_type = growt::AlignedAllocator<>;
  // using allocator_type = growt::GenericAlignedAllocator<>;

  using HashMap =
      typename growt::table_config<StoreKey, StoreValue,
                                   hashingCombined::VectorHasherCast,
                                   allocator_type, hmod::growable>::table_type;

  struct addGistTrue {
    using mapped_type = StoreValue;
    mutable mapped_type previous_value;

    mapped_type operator()(mapped_type &lhs) const {
      // here cancel tasks
      previous_value = lhs;
      lhs = nullptr;//reinterpret_cast<StoreValue>(nullptr);
      return lhs;
    }
    mapped_type atomic(mapped_type &lhs) const {
      previous_value = lhs;
      while (!__sync_bool_compare_and_swap(
          &lhs, previous_value, nullptr)) {
        previous_value = lhs;
      }
      return lhs;
    }
    // todo maybe do an atomic version with compare and swap
    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
  };

  struct addDelayed {
    using mapped_type = StoreValue;
    mutable bool inserted;

    mapped_type operator()(mapped_type &lhs, TaskPointer rhs) const {
      auto lhs_value = reinterpret_cast<Value>(lhs);
      if (lhs_value != nullptr) {
        inserted = true;
        lhs_value = new DelayedTasksList(rhs, lhs_value);
        lhs = reinterpret_cast<StoreValue>(lhs_value);
      } else {
        inserted = false;
      }
      return lhs;
    }

    mapped_type atomic(mapped_type &lhs, TaskPointer rhs) const {
      inserted = false;
      mapped_type expected = lhs;
      while (true) {
        auto lhs_value = reinterpret_cast<Value>(expected);
        if (lhs_value != nullptr) {
          lhs_value = new DelayedTasksList(rhs, lhs_value);
          mapped_type new_value = reinterpret_cast<mapped_type>(lhs_value);
          if (__sync_bool_compare_and_swap(&lhs, expected, new_value)) {
            inserted = true;
            return new_value;
          } else {
            expected = lhs;
          }
        } else {
          return lhs;
        }
      }
    }

    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
  };
  HashMap map_;

public:
  GrowtHashMap(std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage)
      : IConcurrentHashMapCombined(Gist_storage), map_(2000){};

  DelayedTasksList *insert(Key key, bool value) override {
    auto newEntry = createGistEntry(key);
    auto handle = map_.get_handle();
    if (value) {
      addGistTrue functor;

      auto isNew = handle.insert_or_update(
          reinterpret_cast<StoreKey>(newEntry),
          nullptr, functor);
      if (!isNew.second) {
        deleteGistEntry();
      }
      return reinterpret_cast<DelayedTasksList *>(functor.previous_value);
    } else {
      auto isNew = handle.insert(reinterpret_cast<StoreKey>(newEntry),
                                 reinterpret_cast<StoreValue>(-1));
      if (!isNew.second) {
        deleteGistEntry();
      }
      return reinterpret_cast<DelayedTasksList *>(-1);
    }
  }

  void reinsertGist(int *gist, DelayedTasksList *delayed) override {
    auto newEntry = reinterpret_cast<StoreKey>(createGistEntry(gist));
    auto handle = map_.get_handle();
    const auto isNew =
        handle.insert(newEntry, reinterpret_cast<StoreValue>(delayed));
    assert(isNew.second);
  }

  int find(const Key key) override {
    auto handle = map_.get_handle();
    auto temp = handle.find(reinterpret_cast<StoreKey>(key));
    int result = 0;
    if (temp != handle.end()) {
      auto value = reinterpret_cast<DelayedTasksList *>(
          static_cast<StoreValue>((*temp).second));
      result = value == nullptr ? 2 : 1;
    }
    return result;
  }

  bool tryAddDelayed(std::shared_ptr<CustomTask> &task, int *key) override {
    addDelayed functor;

    auto handle = map_.get_handle();
    auto isNew = handle.update(reinterpret_cast<StoreKey>(key), functor, task);
    return isNew.second && functor.inserted;
  }

  std::vector<DelayedTasksList *> getDelayed() override {
    std::vector<DelayedTasksList *> delayedLists;
    auto handle = map_.get_handle();
    auto it = handle.begin();
    while (it != handle.end()) {
      auto entry = *it;
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<StoreValue>(-1)) {
        auto value = reinterpret_cast<DelayedTasksList *>(
            static_cast<StoreValue>(entry.second));
        delayedLists.push_back(value);
      }
      ++it;
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
    auto handle = map_.get_handle();
    auto it = handle.begin();
    while (it != handle.end()) {
      auto entry = *it;
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<StoreValue>(-1)) {
        auto value = reinterpret_cast<DelayedTasksList *>(
            static_cast<StoreValue>(entry.second));
        // check the max offset
        if (reinterpret_cast<Key>(entry.first)[gistLength] >= newOffset) {
          maybeReinsert.push(
              std::make_pair(reinterpret_cast<Key>(entry.first), value));
        } else {
          restart.push(value);
        }
      }
      ++it;
    }
  }

  void clear() override { map_ = HashMap(50); }

  void iterate() const {}
};
