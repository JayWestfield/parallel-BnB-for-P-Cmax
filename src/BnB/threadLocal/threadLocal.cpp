#include "threadLocal.h"

// Define the thread_local variable
thread_local std::vector<int> threadLocalVector;
thread_local int threadIndex;

void initializeThreadLocalVector(int size) {
    if (threadLocalVector.size() != size ) {
        threadLocalVector.resize(size); // need accurate size because of the equal comparator in Vectorhasher
    }
}
