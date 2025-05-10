#pragma once

#include "_refactoredBnB/ST/Hashmaps/hashing/hashing.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include <cassert>

#include <tbb/concurrent_hash_map.h>
#include <utility>
#include <vector>
using namespace ws;
template <bool use_fingerprint>
using tbbMap = tbb::concurrent_hash_map<
    HashKey, HashValue, hashingCombined::VectorHasherPrint<use_fingerprint>>;
template <bool use_fingerprint, bool use_max_offset>
class TBBHashMap_refactored {

public:
  TBBHashMap_refactored(int initialHashmapSize, int maxAllowedParallelism)
      : map_(initialHashmapSize){};

  std::pair<bool, HashValue> addGist(HashKey key) {
    typename tbbMap<use_fingerprint>::accessor accessor;

    const bool isNew = map_.insert(accessor, key);
    const auto delayed = isNew ? nullptr : accessor->second;
    accessor->second = nullptr;
    return std::make_pair(isNew, delayed);
  }

  bool addPreviously(HashKey key) {
    typename tbbMap<use_fingerprint>::accessor accessor;
    const bool isNew = map_.insert(accessor, key);
    if (isNew) {
      accessor->second = reinterpret_cast<HashValue>(-1);
      return true;
    }
    return false;
  }
  bool tryAddDelayed(TaskPointer task, HashKey key) {
    typename tbbMap<use_fingerprint>::accessor accessor;
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
  void reinsertGist(HashKey gist, HashValue delayed) {
    typename tbbMap<use_fingerprint>::accessor accessor;
    const bool isNew = map_.insert(accessor, gist);
    assert(isNew);
    accessor->second = delayed;
  }

  FindGistResult find(const HashKey key) {
    typename tbbMap<use_fingerprint>::accessor accessor;
    if (map_.find(accessor, key)) {
      return accessor->second == nullptr ? FindGistResult::COMPLETED
                                         : FindGistResult::STARTED;
    }
    return FindGistResult::NOT_FOUND;
  }

  std::vector<HashValue> getDelayed() {
    std::vector<HashValue> delayedLists;
    for (auto entry : map_) {
      if (entry.second != nullptr &&
          entry.second != reinterpret_cast<HashValue>(-1)) {
        delayedLists.push_back(entry.second);
      }
    }
    return delayedLists;
  }
  // cannot filter the gist here, because we do not have access to the compute
  // gist necessary to chekc wether the delayed tast
  // first describes entries that might be reinserted
  void getNonEmptyGists(
      tbb::concurrent_queue<std::pair<HashKey, HashValue>> &maybeReinsert,
      tbb::concurrent_queue<HashValue> &restart, const int newOffset) {
    std::pair<tbb::concurrent_queue<std::pair<HashKey, HashValue>>,
              tbb::concurrent_queue<HashValue>>
        nonEmpty;
    for (auto entry : map_) {
      if (isNotEmpty(entry.second)) {
        // check the max offset TODO check is gist length correct ?
        if (use_max_offset && entry.first[ws::gistLength] >= newOffset) {
          maybeReinsert.push(entry);
        } else {
          restart.push(entry.second);
        }
      }
    }
  }
  // todo check for the tbb version the normal iteration might be better?
  void iterateThreadOwnGists(
      int offset, GistStorage storageToIterate,
      tbb::concurrent_queue<std::pair<HashKey, HashValue>> &maybeReinsert,
      tbb::concurrent_queue<HashValue> &restart) {
    // TODO can i reuse the accessor? / is that beneficial?
    typename tbbMap<use_fingerprint>::const_accessor accessor;
    static_assert(!use_fingerprint,
                  "just noticed the iterate methods do not work correctly with "
                  "the fingerprint since the fingerprint hast to be removed "
                  "before adding it to maybereinsert!!!");
    for (auto it : storageToIterate) {
      auto value = map_.find(
          accessor, FingerPrintUtil<use_fingerprint>::addFingerprint(it));
      assert(value);
      // if (isNotEmpty(accessor->second)) {
      if (use_max_offset &&
          FingerPrintUtil<use_fingerprint>::getOriginalPointer(
              accessor->first)[ws::gistLength] >= offset) {
        maybeReinsert.push(*accessor);
      } else if (isNotEmpty(accessor->second)) {
        restart.push(accessor->second);
      }
      // }
    }
  }
  void iterateThreadOwnGistsEvict(
      GistStorage &storageToIterate,
      tbb::concurrent_queue<std::pair<std::vector<int>, HashValue>> &reinsert) {
    // TODO can i reuse the accessor? / is that beneficial?
    typename tbbMap<use_fingerprint>::const_accessor accessor;

    for (auto it : storageToIterate) {
      auto value = map_.find(
          accessor, FingerPrintUtil<use_fingerprint>::addFingerprint(it));
      assert(value);
      if (isNotEmpty(accessor->second)) {
        std::vector<int> gistWrapped(ws::wrappedGistLength);
        // TODO use memcopy / initialize with values
        for (int i = 0; i < ws::wrappedGistLength; ++i) {
          gistWrapped[i] = it[i];
        }
        reinsert.push(std::make_pair(gistWrapped, accessor->second));
      }
    }
  }
  void BoundUpdateClear() { map_.clear(); }

  void clear() { map_.clear(); }

private:
  tbbMap<use_fingerprint> map_;
  inline bool isNotEmpty(HashValue value) const {
    return value != nullptr && value != reinterpret_cast<HashValue>(-1);
  }
};
