#pragma once
#include <vector>
// TODO check the state has to be copied because it was a
// threadstatelocalvectore most likely
enum class Continuation { InitialStart, FUR, BASECASE, DELAYED, STCHECK, END };
struct TaskContext {
  std::vector<int> state;
  int job;
  Continuation continueAt = Continuation::InitialStart;
  std::vector<int> r6 = {};
  int loopIndex = -1;
};