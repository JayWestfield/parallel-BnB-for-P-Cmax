#include "threadLocal.h"

// Define the thread_local variable
thread_local std::vector<int> threadLocalVector;
void initializeThreadLocalVector(int size) {
    if (threadLocalVector.empty()) {
        threadLocalVector.resize(size, 0);
    }
}
