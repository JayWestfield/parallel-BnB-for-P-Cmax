#pragma once
// #include <growt/alignedallocator.hpp>
#include "./CustomUint64.hpp"
#include "_refactoredBnB/ST/Hashmaps/hashing/hashing.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/GistStorage.hpp"
#include "_refactoredBnB/Structs/TaskContext.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include "allocator/alignedallocator.hpp"
#include "data-structures/hash_table_mods.hpp"
#include "data-structures/migration_table.hpp"
#include "data-structures/table_config.hpp"
#include "src/ws_common.hpp"
// #include "hash_table_mods.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <oneapi/tbb/concurrent_queue.h>
#include <queue>
#include <string>
// #include <table_config.h>
// #include <growt>

#include <unordered_map>
#include <utility>
#include <vector>

// TODO update to use the DelayedTasksList
// TODO check muss ich das value ding auch als reinterpret cast machen? weil ich
// gklaube das wird als pointer gewrapped

template <bool use_fingerprint> class GrowtHashMap_refactored {
public:
  using Key = int *;
  // TODO find a way to  the equal method of the store key while using
  // uint64
  using StoreKey = CustomUint64<use_fingerprint>;
  using TempConversionValue = uint64_t;

  using Value = DelayedTasksList *;
  // evtl geht das acuh oghne das reinterpret cast (für value) glaube das
  // problme war das fehlende atomic
  using StoreValue = Value; // uint64_t;
  static_assert(sizeof(StoreKey) == sizeof(TempConversionValue),
                "CustomUint64 must have the same size as uint64_t");

  using TaskPointer = Task<TaskContext> *;
  using allocator_type = growt::AlignedAllocator<>;
  // using allocator_type = growt::GenericAlignedAllocator<>;

  using HashMap = typename growt::table_config<
      CustomUint64<use_fingerprint>, StoreValue,
      hashingCombined::VectorHasherCast<use_fingerprint>, allocator_type,
      hmod::growable, hmod::deletion>::table_type;

  struct addGistTrue {
    using mapped_type = StoreValue;
    mapped_type &previous_value;
    addGistTrue(mapped_type &previous_value) : previous_value(previous_value){};
    mapped_type operator()(mapped_type &lhs) const {
      previous_value = lhs;
      lhs = nullptr; // reinterpret_cast<StoreValue>(nullptr);
      return lhs;
    }
    // mapped_type atomic(mapped_type &lhs) const {
    //   previous_value = lhs;
    //   assert(lhs == reinterpret_cast<Value>(-1) || lhs == nullptr ||
    //          previous_value->value->context.state.size() == ws::gistLength);
    //   while (!__sync_bool_compare_and_swap(&lhs, previous_value, nullptr)) {
    //     previous_value = lhs;
    //   }
    //   return lhs;
    // }
    // todo maybe do an atomic version with compare and swap
    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
  };

  struct addDelayed {
    using mapped_type = StoreValue;
    bool &inserted;
    addDelayed(bool &inserted) : inserted(inserted){};
    mapped_type operator()(mapped_type &lhs, TaskPointer rhs) {
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
    // atomic update leads to issues somehow
    // StoreValue atomic(mapped_type &lhs, TaskPointer rhs) {
    //   mapped_type expected = lhs;
    //   while (true) {
    //     auto lhs_value = reinterpret_cast<Value>(expected);
    //     if (lhs_value != nullptr) {
    //       // TODO check that might create a memory leak
    //       lhs_value = new DelayedTasksList(rhs, lhs_value);
    //       mapped_type new_value = reinterpret_cast<mapped_type>(lhs_value);
    //       if (__sync_bool_compare_and_swap(&lhs, expected, lhs_value)) {
    //         inserted = true;
    //         return lhs_value;
    //       } else {
    //         expected = lhs;
    //         lhs_value->next = nullptr;
    //         delete lhs_value;
    //       }
    //     } else {
    //       return lhs;
    //     }
    //   }
    // }

    // Only necessary for JunctionWrapper (not needed)
    using junction_compatible = std::true_type;
  };
  HashMap map_;
  uint64_t size = 0;
  inline StoreKey castStoreKey(Key key) const {
    return reinterpret_cast<TempConversionValue>(key);
  }

public:
  GrowtHashMap_refactored(int initialHashmapSize, int maxAllowedParallelism)
      : map_(initialHashmapSize), handles() {
    for (int i = 0; i < maxAllowedParallelism; i++) {
      handles.push_back(map_.get_handle());
    }
  };
  ~GrowtHashMap_refactored() { handles.clear(); };
  std::pair<bool, DelayedTasksList *> addGist(Key key) {
    StoreKey newEntry = castStoreKey(key);
    auto &handle = handles[ws::thread_index_];
    Value previous_value = reinterpret_cast<Value>(-1);
    addGistTrue functor(previous_value);
    auto isNew = handle.insert_or_update(newEntry, nullptr, functor);
    // true signals insertion => keep gistEntry

    assert(static_cast<Value>((*(isNew.first)).second) == nullptr);
    assert(previous_value == reinterpret_cast<Value>(-1) ||
           previous_value == nullptr ||
           previous_value->value->context.state.size() == ws::gistLength - 1);

    return std::make_pair(isNew.second,
                          reinterpret_cast<DelayedTasksList *>(previous_value));
  }
  bool addPreviously(Key key) {
    auto &handle = handles[ws::thread_index_];
    StoreKey newEntry = castStoreKey(key);

    auto isNew = handle.insert(newEntry, reinterpret_cast<StoreValue>(-1));
    return isNew.second;
  }
  DelayedTasksList *insert(Key key, bool value) {
    assert(false && "unused/deprecated");
    StoreKey newEntry = castStoreKey(key);
    auto &handle = handles[ws::thread_index_];
    if (value) {
      Value previous_value = nullptr;
      addGistTrue functor(previous_value);
      auto isNew = handle.insert_or_update(newEntry, nullptr, functor);
      // {
      //   auto testEntry = reinterpret_cast<TempConversionValue>(
      //       reinterpret_cast<uintptr_t>(createGistEntry(GistStorage,key)));
      //   auto hasher = hashingCombined::VectorHasherCast();
      //   // assert(hasher.equal(newEntry,
      //   // static_cast<StoreKey>((*(isNew.first)).first)));
      //   // assert(hasher.equal(static_cast<StoreKey>((*(isNew.first)).first),
      //   //                     reinterpret_cast<StoreKey>(testEntry)));
      //   // assert(hasher.equal(newEntry,
      //   // reinterpret_cast<StoreKey>(testEntry)));
      //   assert(handle.find(newEntry) != handle.end());
      //   assert(handle.find(testEntry) != handle.end());
      //   deleteGistEntry();
      // }
      if (isNew.second == false) {
        // deleteGistEntry(Gist_Storage);
      }
      assert(static_cast<Value>((*(isNew.first)).second) == nullptr);
      assert(previous_value == reinterpret_cast<Value>(-1) ||
             previous_value == nullptr ||
             previous_value->value->context.state.size() == ws::gistLength - 1);

      return reinterpret_cast<DelayedTasksList *>(previous_value);
    } else {
      auto isNew = handle.insert(newEntry, reinterpret_cast<StoreValue>(-1));
      if (isNew.second == false) {
        // deleteGistEntry(Gist_Storage);
      }
      return reinterpret_cast<DelayedTasksList *>(-1);
    }
  }

  void reinsertGist(int *gist, DelayedTasksList *delayed) {
    auto newEntry = castStoreKey(gist);
    auto &handle = handles[ws::thread_index_];
    const auto isNew = handle.insert(newEntry, delayed);
    assert(isNew.second);
  }

  FindGistResult find(const Key key) {
    auto &handle = handles[ws::thread_index_];
    auto temp = handle.find(castStoreKey(key));
    FindGistResult result = FindGistResult::NOT_FOUND;
    if (temp != handle.end()) {
      auto value = reinterpret_cast<DelayedTasksList *>(
          static_cast<StoreValue>((*temp).second));
      result = value == nullptr ? FindGistResult::COMPLETED
                                : FindGistResult::STARTED;
    }
    return result;
  }

  bool tryAddDelayed(TaskPointer task, int *key) {
    bool inserted = false;
    addDelayed functor = addDelayed(inserted);

    auto &handle = handles[ws::thread_index_];
    auto isNew = handle.update(castStoreKey(key), functor, task);
    auto iterated = (isNew.first);
    auto elem = *iterated;
    auto value = reinterpret_cast<Value>(static_cast<StoreValue>(elem.second));
    assert(value == nullptr || inserted);
    return isNew.second && inserted;
  }
  // TODO get delayed can be done like iterate own gists but needs work ofc
  std::vector<DelayedTasksList *> getDelayed() {
    std::vector<DelayedTasksList *> delayedLists;
    auto &handle = handles[ws::thread_index_];
    auto it = handle.begin();
    while (it != handle.end()) {
      auto entry = *it;
      if (static_cast<Value>(entry.second) != nullptr &&
          static_cast<Value>(entry.second) !=
              reinterpret_cast<StoreValue>(-1)) {
        auto value = reinterpret_cast<DelayedTasksList *>(
            static_cast<StoreValue>(static_cast<Value>(entry.second)));
        delayedLists.push_back(value);
      }
      // std::cout << static_cast<StoreKey>(entry.first) << std::endl;
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
                   const int newOffset) {
    size = 0;
    std::pair<tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>,
              tbb::concurrent_queue<DelayedTasksList *>>
        nonEmpty;
    // std::queue<std::pair<int *, DelayedTasksList *>> found;
    auto &handle = handles[ws::thread_index_];
    auto it = handle.begin();
    for (auto es : handle) {
      size++;
    }

    for (; it != handle.end(); ++it) {
      size++;
      auto entry = *it;
      if (static_cast<Value>(entry.second) != nullptr &&
          static_cast<Value>(entry.second) !=
              reinterpret_cast<StoreValue>(-1)) {
        auto value = reinterpret_cast<DelayedTasksList *>(
            static_cast<StoreValue>(static_cast<Value>(entry.second)));
        // check the max offset
        // found.push(std::make_pair(
        //     reinterpret_cast<Key>(static_cast<StoreKey>(entry.first).value),
        //     value));
        if (reinterpret_cast<Key>(
                static_cast<StoreKey>(entry.first).value)[ws::gistLength] >=
            newOffset) {
          maybeReinsert.push(std::make_pair(
              reinterpret_cast<Key>(static_cast<StoreKey>(entry.first).value),
              value));
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
    // TODO maybe a handle per gist ist better du e to independence of loops/
    // unrolling
    auto &handle = handles[ws::thread_index_];
    for (auto gist : storageToIterate) {
      auto temp = handle.find(castStoreKey(gist));
      assert(temp != handle.end());
      auto entry = *temp;
      if (static_cast<Value>(entry.second) != nullptr &&
          static_cast<Value>(entry.second) !=
              reinterpret_cast<StoreValue>(-1)) {
        auto value = reinterpret_cast<DelayedTasksList *>(
            static_cast<StoreValue>(static_cast<Value>(entry.second)));
        // check the max offset
        // found.push(std::make_pair(
        //     reinterpret_cast<Key>(static_cast<StoreKey>(entry.first).value),
        //     value));
        if (reinterpret_cast<Key>(
                static_cast<StoreKey>(entry.first).value)[ws::gistLength] >=
            offset) {
          maybeReinsert.push(std::make_pair(
              reinterpret_cast<Key>(static_cast<StoreKey>(entry.first).value),
              value));
        } else {
          restart.push(value);
        }
      }
      // try erase every element instead of recreating the map
      auto val = temp.erase_if_unchanged();
      // std::cout << val << std::endl;
      assert(val);
      assert(map_.get_handle().find(castStoreKey(gist)) ==
             map_.get_handle().end());
    }
    cleared = true;
  }
  void BoundUpdateClear() {
    auto begin = map_.get_handle().begin();
    if (begin != map_.get_handle().end()) {
      auto elem = *begin;
      std::cout << reinterpret_cast<Key>(((elem.first)).value) << std::endl;
      for (int i = 0; i < ws::gistLength + 1; ++i) {
        std::cout << *(reinterpret_cast<Key>(((elem.first)).value) + i) << ", ";
      }
      std::cout << std::endl << elem.second << std::endl;
    }
    assert(map_.get_handle().begin() == map_.get_handle().end());
  }
  void clear() {
    // map_ = HashMap(5000000);
    // if (handles[ws::thread_index_].begin() !=
    // handles[ws::thread_index_].end()) {
    if (!cleared) {
      const int handlecount = handles.size();
      handles.clear();
      map_ = HashMap(5000);
      // update handles
      for (int i = 0; i < handlecount; i++) {
        handles.push_back(map_.get_handle());
      }
    }
    assert(map_.get_handle().begin() == map_.get_handle().end());
    cleared = false;
  }

  void iterate() const {}
  bool cleared = false;

private:
  std::vector<typename HashMap::handle_type> handles;
};
