#pragma once
#include "../external/task-based-workstealing/src/work_stealing_config.hpp"

#include "_refactoredBnB/Structs/TaskContext.hpp"
#include <memory>
struct DelayedTasksList {
  Task<TaskContext> *value;
  DelayedTasksList *next;
  DelayedTasksList(Task<TaskContext> *val, DelayedTasksList *next = nullptr)
      : value(val), next(next){};
  ~DelayedTasksList() {
    if (next != nullptr && next != reinterpret_cast<DelayedTasksList *>(-1))
      delete next;
  }
};