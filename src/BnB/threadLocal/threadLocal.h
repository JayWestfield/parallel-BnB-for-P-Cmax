#ifndef THREADLOCALVECTOR_H
#define THREADLOCALVECTOR_H
#include <vector>

// Declare the thread_local variable
extern thread_local std::vector<int> threadLocalVector;
extern thread_local int threadIndex;

void initializeThreadLocalVector(int size);
#endif // THREADLOCALVECTOR_H