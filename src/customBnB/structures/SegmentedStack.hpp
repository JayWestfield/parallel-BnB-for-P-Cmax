#include "STEntry.hpp"
#include <cassert>
#include <vector>
#pragma once
template <std::size_t SegmentSize = 1024> class SegmentedStack {
public:
  SegmentedStack() : current_index(0), segments() {
    segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
  }

  STEntry *push(int *gist, bool finished) {
    if (current_index == SegmentSize) {
      segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
      current_index = 0;
    }
    STEntry &newEntry = (*segments.back())[current_index++];
    newEntry = STEntry(gist, finished);
    return &newEntry;
  }
  void pop() {
    assert(current_index != 0);
    current_index--;
  }

  void clear() {
    // assert delyed tasks are already resumed
    segments.clear();
    segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
  }
  class iterator {
  public:
    iterator(SegmentedStack *stack, std::size_t segment_idx,
             std::size_t entry_idx)
        : stack(stack), segment_idx(segment_idx), entry_idx(entry_idx) {}

    iterator &operator++() {
      if (entry_idx + 1 < SegmentSize &&
          (segment_idx < stack->segments.size() - 1 ||
           entry_idx + 1 < stack->current_index)) {
        ++entry_idx;
      } else if (segment_idx + 1 < stack->segments.size()) {
        ++segment_idx;
        entry_idx = 0;
      } else {
        segment_idx = stack->segments.size();
        entry_idx = 0;
      }
      return *this;
    }

    bool operator!=(const iterator &other) const {
      return segment_idx != other.segment_idx || entry_idx != other.entry_idx;
    }

    STEntry &operator*() { return (*stack->segments[segment_idx])[entry_idx]; }

  private:
    SegmentedStack *stack;
    std::size_t segment_idx;
    std::size_t entry_idx;
  };

  iterator begin() { return iterator(this, 0, 0); }

  iterator end() { return iterator(this, segments.size(), 0); }

private:
  std::vector<std::unique_ptr<std::array<STEntry, SegmentSize>>> segments;
  std::size_t current_index;
};