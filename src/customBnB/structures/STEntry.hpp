#pragma once
#include "DelayedTasksList.hpp"
#include <vector>
// two cases for none, nullpointer => gist is finished and pointer to -1 means empty will be a single linked lsit to cehck use the util function
struct STEntry {
  std::vector<int> gist;
  DelayedTasksList *taskList;
  STEntry(std::vector<int> &gist, bool finished): gist(gist) {
    taskList = finished ? nullptr : reinterpret_cast<DelayedTasksList *>(-1);
  }
};