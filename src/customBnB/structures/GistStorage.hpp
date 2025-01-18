#include "customBnB/structures/SegmentedStack.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>
#pragma once
extern int gistLength;
extern int wrappedGistLength;

template <std::size_t SegmentSize = 1024> class GistStorage {
public:
  GistStorage() : current_index(0), segments() {
    segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
  }
  // GistStorage(const GistStorage&) = delete;

  ~GistStorage() = default;
  int *push(int *gist) {
    if (current_index >= SegmentSize - wrappedGistLength) {
      segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
      current_index = 0;
    }
    // TODO compute pointer instead of &
    int *begin = &((*segments.back())[current_index]);
    // copy the initial gist TODO it might be more efficient to directly create
    // that in there but that is bad for decoupling the hashmap from the stclass
    // TODO rather use a memfill or sth like that
    for (size_t i = 0; i < static_cast<size_t>(wrappedGistLength); i++) {
      (*segments.back())[current_index++] = *(gist + i);
    }
    return begin;
  }
  void pop() {
    assert(current_index != 0);
    current_index -= wrappedGistLength;
  }

  void clear() {
    // assert delyed tasks are already resumed
    segments.clear();
    segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
    current_index = 0;
    assert(segments.size() == 1 && current_index == 0);
  }
  // might not need to be iterable only if the hashtable is not iterable or if
  // we have no collection of all the delayed task (again a segmented stack?)
  // is an index for segments better that to use .back ?
  class Iterator {
  public:
    Iterator(
        std::size_t seg_idx, std::size_t idx,
        std::vector<std::unique_ptr<std::array<int, SegmentSize>>> &segments,
        std::size_t current_index)
        : seg_it(seg_idx), idx(idx), segments(segments),
          current_index(current_index) {}

    // int *operator*() const { return &((*segments[seg_it])[idx]); }
    int *operator*() const { return (*segments[seg_it]).begin() + idx; }

    Iterator &operator++() {
      idx += wrappedGistLength;
      if (seg_it == segments.size() - 1 && idx == current_index) {
        return *this;
      }
      if (idx >= SegmentSize - wrappedGistLength && seg_it <= segments.size()) {
        ++seg_it;
        idx = 0;
      }
      assert(seg_it < segments.size());
      assert(idx < SegmentSize - wrappedGistLength);
      assert(seg_it < segments.size() - 1 || idx <= current_index);
      return *this;
    }

    bool operator!=(const Iterator &other) const {
      return seg_it != other.seg_it || idx != other.idx;
    }

  private:
    std::vector<std::unique_ptr<std::array<int, SegmentSize>>> &segments;
    std::size_t current_index;
    std::size_t seg_it;
    std::size_t idx;
  };

  Iterator begin() { return Iterator(0, 0, segments, current_index); }

  Iterator end() {
    return Iterator(segments.size() - 1, current_index, segments,
                    current_index);
  }

private:
  std::size_t current_index;
  std::vector<std::unique_ptr<std::array<int, SegmentSize>>> segments;
};