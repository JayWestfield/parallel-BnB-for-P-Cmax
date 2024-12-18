#include "threadLocal.h"

// Define the thread_local variable
thread_local std::vector<int> threadLocalVector;
thread_local std::vector<int> threadLocalStateVector;

thread_local int threadIndex;

void initializeThreadLocalVector(int size) {
  threadLocalVector.resize(size + 2); // need accurate size because of the equal
                                  // comparator in Vectorhasher
  threadLocalStateVector.resize(size);
}
