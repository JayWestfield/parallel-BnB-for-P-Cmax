#pragma once
#include "DelayedTasksList.hpp"
#include <vector>
// two cases for none, nullpointer => gist is finished and pointer to -1 means
// empty will be a single linked lsit to cehck use the util function
// TODO replace vector with raw data pointer and add deconstructor that handles
// deletion
extern int gistLength;
class STEntry {
public:
  std::vector<int> gist;
  DelayedTasksList *taskList;
  STEntry() {
  }
  STEntry(int *gist, bool finished) {
    this->gist = std::vector<int>(gist, gist + gistLength);
    taskList = finished ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
  }
};