#pragma once
#include "structures/TaskHolder.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>
#include <tuple>
#include <vector>

// note since this task group should persist when a task is canceled it should
// be created with a shared pointer
class CustomTaskGroup : public std::enable_shared_from_this<CustomTaskGroup> {
public:
  // todo ad suspend
  CustomTaskGroup(ITaskHolder &taskHolder, std::shared_ptr<CustomTask> creator)
      : taskHolder(taskHolder), creator(creator){};
  void run(std::vector<int> &state, int job) {
    this->runnableChildren.push_back(std::make_tuple(state, job));
    // this->help.push_back(std::make_tuple(state, job));

    totalChildren++;
  }
  bool wait(int continueAt) { // has to return in code if wait is true
                              // unregisterChild is responsible for adding the
                              // task again if it is ready
    creator->continueAt = continueAt;
    bool hasChildren = finishedChildren != totalChildren;
    assert(static_cast<size_t>(totalChildren) == finishedChildren + runnableChildren.size());
    for (auto child : runnableChildren) {
      taskHolder.addTask(std::make_shared<CustomTask>(
          std::get<0>(child), std::get<1>(child), shared_from_this()));
    }
    runnableChildren.clear();
    return hasChildren;
  }

  void unregisterChild() {
    // TODO use fetch add
    int new_value = ++finishedChildren;

    assert(new_value <= totalChildren);

    if (new_value == totalChildren) {
      // Race conditions with the wait method
      // TODO FIX error the task might not have been paused and if it has not
      // been paused then the add task duplicates it !!!!!!!!!!!
      taskHolder.addTask(creator);
    }
  }

private:
  std::atomic<int> finishedChildren = 0;
  std::atomic<int> totalChildren = 0;
  std::vector<std::tuple<std::vector<int>, int>> runnableChildren;
  // std::vector<std::tuple<std::vector<int>, int>> help;

  ITaskHolder &taskHolder;
  std::shared_ptr<CustomTask> creator;
};