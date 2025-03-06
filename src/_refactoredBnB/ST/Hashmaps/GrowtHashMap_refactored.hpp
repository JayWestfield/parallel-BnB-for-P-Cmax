#pragma once

#include "./CustomUint64.hpp"
#include "_refactoredBnB/ST/Hashmaps/hashing/hashing.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/GistStorage.hpp"
#include "_refactoredBnB/Structs/TaskContext.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include "allocator/alignedallocator.hpp"
#include "data-structures/hash_table_mods.hpp"
#include "data-structures/table_config.hpp"
#include <cassert>
#include <cstdint>
#include <oneapi/tbb/concurrent_queue.h>

#include <utility>
#include <vector>
using namespace ws;

// TODO update to use the DelayedTasksList
// TODO check muss ich das value ding auch als reinterpret cast machen? weil ich
// gklaube das wird als pointer gewrapped
template <bool use_fingerprint> class GrowtHashMap_refactored {
  using StoreKey = CustomUint64<use_fingerprint>;

  // TODO find a way to  the equal method of the store key while using
  // uint64
  using TempConversionValue = uint64_t;

  static_assert(sizeof(StoreKey) == sizeof(TempConversionValue),
                "CustomUint64 must have the same size as uint64_t");

  const HashValue emptyList = emptyList;
  using allocator_type = growt::AlignedAllocator<>;
  // using allocator_type = growt::GenericAlignedAllocator<>;

  using HashMap = typename growt::table_config<
      StoreKey, HashValue, hashingCombined::VectorHasherCast<use_fingerprint>,
      allocator_type, hmod::growable, hmod::deletion>::table_type;

  struct addGistTrue {
    using mapped_type = HashValue;
    mapped_type &previous_value;
    addGistTrue(mapped_type &previous_value) : previous_value(previous_value){};
    mapped_type operator()(mapped_type &lhs) const {
      previous_value = lhs;
      lhs = nullptr;
      return lhs;
    }
    mapped_type atomic(mapped_type &lhs) const {
      previous_value = lhs;
      assert(lhs == emptyList || lhs == nullptr ||
             previous_value->value->context.state.size() == gistLength);
      while (!__sync_bool_compare_and_swap(&lhs, previous_value, nullptr)) {
        previous_value = lhs;
      }
      return lhs;
    }
  };

  struct addDelayed {
    using mapped_type = HashValue;
    bool &inserted;
    addDelayed(bool &inserted) : inserted(inserted){};
    mapped_type operator()(mapped_type &lhs, TaskPointer rhs) {
      auto lhs_value = reinterpret_cast<HashValue>(lhs);
      if (lhs_value != nullptr) {
        inserted = true;
        lhs_value = new DelayedTasksList(rhs, lhs_value);
        lhs = reinterpret_cast<HashValue>(lhs_value);
      } else {
        inserted = false;
      }
      return lhs;
    }
    HashValue atomic(mapped_type &lhs, TaskPointer rhs) {
      mapped_type expected = lhs;
      while (true) {
        auto lhs_value = reinterpret_cast<HashValue>(expected);
        if (lhs_value != nullptr) {
          lhs_value = new DelayedTasksList(rhs, lhs_value);
          mapped_type new_value = reinterpret_cast<mapped_type>(lhs_value);
          if (__sync_bool_compare_and_swap(&lhs, expected, lhs_value)) {
            inserted = true;
            return lhs_value;
          } else {
            expected = lhs;
            lhs_value->next = nullptr;
            delete lhs_value;
          }
        } else {
          return lhs;
        }
      }
    }
  };

public:
  GrowtHashMap_refactored(int initialHashmapSize, int maxAllowedParallelism)
      : map_(initialHashmapSize), handles(),
        initialHashmapSize_(initialHashmapSize) {
    for (int i = 0; i < maxAllowedParallelism; i++) {
      handles.push_back(map_.get_handle());
    }
  };

  ~GrowtHashMap_refactored() { handles.clear(); };
  std::pair<bool, HashValue> addGist(HashKey key) {
    HashValue previous_value = emptyList;
    auto result = getHandle().insert_or_update(castStoreKey(key), nullptr,
                                               addGistTrue(previous_value));

    assert(static_cast<HashValue>((*(result.first)).second) == nullptr);
    assert(previous_value == emptyList || previous_value == nullptr ||
           previous_value->value->context.state.size() == gistLength - 1);

    // true signals insertion => keep gistEntry
    return std::make_pair(result.second,
                          reinterpret_cast<DelayedTasksList *>(previous_value));
  }

  bool addPreviously(HashKey key) {
    auto result = getHandle().insert(castStoreKey(key), emptyList);
    return result.second;
  }

  void reinsertGist(HashKey gist, HashValue delayed) {
    const auto result = getHandle().insert(castStoreKey(gist), delayed);
    assert(result.second);
  }

  FindGistResult find(const HashKey key) {
    auto entryHandle = getHandle().find(castStoreKey(key));
    FindGistResult result = FindGistResult::NOT_FOUND;
    if (entryHandle != getHandle().end()) {
      auto value = reinterpret_cast<DelayedTasksList *>(
          static_cast<HashValue>((*entryHandle).second));
      result = value == nullptr ? FindGistResult::COMPLETED
                                : FindGistResult::STARTED;
    }
    return result;
  }

  bool tryAddDelayed(TaskPointer task, HashKey key) {
    bool inserted = false;
    auto result =
        getHandle().update(castStoreKey(key), addDelayed(inserted), task);
    assert((*(result.first)).second == nullptr || inserted);
    return result.second && inserted;
  }

  // get Delayed is only used for debugging !!!!
  // TODO get delayed can be done like iterate own gists but needs work ofc
  std::vector<DelayedTasksList *> getDelayed() {
    std::vector<DelayedTasksList *> delayedLists;
    auto it = getHandle().begin();
    while (it != getHandle().end()) {
      auto entry = *it;
      HashValue value = entry.second;
      if (isNotEmpty(value)) {
        delayedLists.push_back(value);
      }
      ++it;
    }
    return delayedLists;
  }

  // iterator based version is really slow for Growt => iterateThreadOwnGists
  void
  getNonEmptyGists(tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
                       &maybeReinsert,
                   tbb::concurrent_queue<DelayedTasksList *> &restart,
                   const int newOffset) {
    std::pair<tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>,
              tbb::concurrent_queue<DelayedTasksList *>>
        nonEmpty;
    auto &handle = handles[thread_index_];
    auto it = handle.begin();

    for (; it != handle.end(); ++it) {
      HashKey key =
          reinterpret_cast<HashKey>(static_cast<StoreKey>((*it).first).value);
      HashValue value = (*it).second;

      if (isNotEmpty(value)) {
        if (key[gistLength] >= newOffset) {
          maybeReinsert.push(std::make_pair(key, value));
        } else {
          restart.push(value);
        }
      }
    }
  }

  void iterateThreadOwnGists(
      int offset, GistStorage storageToIterate,
      tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
          &maybeReinsert,
      tbb::concurrent_queue<DelayedTasksList *> &restart) {

    auto &handle = handles[thread_index_];
    for (auto gist : storageToIterate) {
      // TODO use Fingerptint
      auto entryHandle = handle.find(castStoreKey(gist));
      assert(temp != handle.end());
      HashKey key = reinterpret_cast<HashKey>(
          static_cast<StoreKey>((*entryHandle).first).value);
      HashValue value = (*entryHandle).second;
      if (isNotEmpty(value)) {
        if (key[gistLength] >= offset) {
          maybeReinsert.push(std::make_pair(key, value));
        } else {
          restart.push(value);
        }
      }
      // erase every element instead of recreating the map
      // force erase does not work on growt????
      bool val = entryHandle.erase_if_unchanged();

      assert(val);
      assert(map_.get_handle().find(castStoreKey(gist)) ==
             map_.get_handle().end());
    }
  }
  void BoundUpdateClear() {
    // expect map to be empty after each thread iterated and remove its key's
    assert(map_.get_handle().begin() == map_.get_handle().end());
  }
  void clear() {
    const int handlecount = handles.size();
    handles.clear();
    map_ = HashMap(initialHashmapSize_);
    // update handles
    for (int i = 0; i < handlecount; i++) {
      handles.push_back(map_.get_handle());
    }
    assert(map_.get_handle().begin() == map_.get_handle().end());
  }

  void iterate() const {}

private:
  std::vector<typename HashMap::handle_type> handles;
  HashMap map_;
  int initialHashmapSize_;

  inline StoreKey castStoreKey(HashKey key) const {
    return reinterpret_cast<TempConversionValue>(key);
  }
  inline typename HashMap::handle_type &getHandle() {
    return handles[thread_index_];
  }
  inline bool isNotEmpty(HashValue value) const {
    return value != nullptr && value != emptyList;
  }
};
