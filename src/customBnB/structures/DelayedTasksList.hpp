#include "CustomTask.hpp"
#pragma once
struct DelayedTasksList {
  std::shared_ptr<CustomTask> value;
  DelayedTasksList *next;
  DelayedTasksList(std::shared_ptr<CustomTask> &val,
                   DelayedTasksList *next = nullptr)
      : value(val), next(next){};
  ~DelayedTasksList() {
    if (next != nullptr && next != reinterpret_cast<DelayedTasksList *>(-1))
      delete next;
  }
};