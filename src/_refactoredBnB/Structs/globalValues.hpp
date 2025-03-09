#pragma once
#include "../../external/task-based-workstealing/src/ws_common.hpp"
#include "_refactoredBnB/Structs/DelayedTasksList.hpp"
#include "_refactoredBnB/Structs/TaskContext.hpp"
#include <cstddef>
#include <vector>
namespace ws {
thread_local std::vector<int> threadLocalVector;
thread_local std::vector<int> threadLocalStateVector;
std::size_t gistLength = 0;
std::size_t stateLength = 0;
std::size_t wrappedGistLength = 0;
std::size_t maxThreads = 0;
void initializeThreadLocalVector(std::size_t size) {
  threadLocalVector.resize(size + 2); // need accurate size because of the equal
                                      // comparator in Vectorhasher
  threadLocalStateVector.resize(size);
}
using TaskPointer = Task<TaskContext> *;
using HashKey = int *;
using HashValue = DelayedTasksList *;
} // namespace ws
