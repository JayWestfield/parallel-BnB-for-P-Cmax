#pragma once
#include <vector>
// TODO check the state has to be copied because it was a
// threadstatelocalvectore most likely
enum class Continuation { InitialStart, FUR, BASECASE, DELAYED, STCHECK, END };
struct TaskContext {
  std::vector<int> state;
  int job;
  std::vector<int> unsortedState = {};
  int sameJobsize = -1;
  Continuation continueAt = Continuation::InitialStart;
  std::vector<int> r6 = {};
  int loopIndex = -1;
};