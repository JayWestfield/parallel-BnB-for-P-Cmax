#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <vector>
#pragma once
extern int gistLength;

template <std::size_t SegmentSize = 1024> class GistStorage {
public:
  GistStorage() : current_index(0), segments() {
    segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
  }

  int *push(int *gist) {
    if (current_index >= SegmentSize - gistLength) {
      segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
      current_index = 0;
    }
    int *begin = &((*segments.back())[current_index]);
    // copy the initial gist TODO it might be more efficient to directly create that in there but that is bad for decoupling the hashmap from the stclass
    for (size_t  i = 0; i < gistLength; i++) {
      (*segments.back())[current_index++] = *(gist + i);
    }
    return begin;
  }
  void pop() {
    assert(current_index != 0);
    current_index -= gistLength;
  }

  void clear() {
    // assert delyed tasks are already resumed
    segments.clear();
    segments.push_back(std::make_unique<std::array<int, SegmentSize>>());
  }
  // might not need to be iterable only if the hashtable is not iterable or if we have no collection of all the delayed task (again a segmented stack?)
// is an index for segments better that to use .back ?
private:
  std::vector<std::unique_ptr<std::array<int, SegmentSize>>> segments;
  std::size_t current_index;
};