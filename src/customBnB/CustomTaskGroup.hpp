#include "structures/TaskHolder.hpp"
#include <atomic>
#include <cassert>
#include <iostream>
#include <memory>
#include <vector>
#include <sstream>

// note since this task group should persist when a task is canceled it should
// be created with a shared pointer
class CustomTaskGroup : public std::enable_shared_from_this<CustomTaskGroup> {
public:
  // todo ad suspend
  CustomTaskGroup(ITaskHolder &taskHolder, std::shared_ptr<CustomTask> creator)
      : taskHolder(taskHolder), creator(creator), isWaiting(false){};
  void run(std::vector<int> &state, int job) {
    refCounter++;
    taskHolder.addTask(
        std::make_shared<CustomTask>(state, job, shared_from_this()));
  }
  bool wait(int continueAt) { // has to return in code if wait is true
                              // unregisterChild is responsible for adding the
                              // task again if it is ready
    creator->continueAt = continueAt;
    assert(isWaiting == false);
    // invariant ref counter is not incremented while in this method (but might be decremented)
    if (refCounter > 0) {
      isWaiting = true;
      // race condition with the unregister child
      // 2 gegenseitige 
      if (refCounter == 0 && isWaiting) {
        isWaiting = false;
        return false;
      }
      return true;
    }
    return false;
  }

  void unregisterChild() {
    int old_value;
    int new_value;
    do {
      old_value = refCounter.load();
      new_value = old_value - 1;
    } while (!refCounter.compare_exchange_weak(old_value, new_value));

    assert(new_value >= 0);

    if (new_value == 0 && isWaiting) {
      // Race conditions with the wait method
      // TODO FIX error the task might not have been paused and if it has not been paused then the add task duplicates it !!!!!!!!!!!
      isWaiting = false;
      taskHolder.addTask(creator);
    }
    // refCounter--;
    // if (refCounter == 0) { // be carefull with the race condition better do
    // // it with a compare exchange strong otherwise the task might get adde
    // twice which would not only lead to performance issues but to a real bug
    // }
  }

private:
  std::atomic<int> refCounter = 0;
  ITaskHolder &taskHolder;
  std::shared_ptr<CustomTask> creator;
  std::atomic<bool> isWaiting = false;
};