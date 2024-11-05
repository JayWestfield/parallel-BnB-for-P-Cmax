#include "STEntry.hpp"
#include <vector>
template <std::size_t SegmentSize = 1024> 
class SegmentedStack {
public:
  SegmentedStack() : current_index(0) {
    segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
  }

  void push(const STEntry &value) {
    if (current_index == SegmentSize) {
      segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
      current_index = 0;
    }
    (*segments.back())[current_index++] = value;
  }

  void clear() {
    // assert delyed tasks are already resumed
    segments.clear();
    segments.push_back(std::make_unique<std::array<STEntry, SegmentSize>>());
  }

private:
  std::vector<std::unique_ptr<std::array<STEntry, SegmentSize>>> segments;
  std::size_t current_index;
};