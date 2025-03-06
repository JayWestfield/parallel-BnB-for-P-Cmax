#pragma once
#include "../../external/task-based-workstealing/src/ws_common.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/TaskContext.hpp"
#include <vector>
namespace ws {
thread_local std::vector<int> threadLocalVector;
thread_local std::vector<int> threadLocalStateVector;
int gistLength = 0;
int stateLength = 0;
int wrappedGistLength = 0;
int maxThreads = 0;
void initializeThreadLocalVector(int size) {
  threadLocalVector.resize(size + 2); // need accurate size because of the equal
                                      // comparator in Vectorhasher
  threadLocalStateVector.resize(size);
}
using TaskPointer = Task<TaskContext> *;
using HashKey = int *;
using HashValue = DelayedTasksList *;
} // namespace ws
