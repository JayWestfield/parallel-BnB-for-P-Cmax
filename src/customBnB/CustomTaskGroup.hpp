#include "structures/TaskHolder.hpp"
#include <atomic>
#include <memory>
#include <vector>

// note since this task group should persist when a task is canceled it should
// be created with a shared pointer
class CustomTaskGroup : public std::enable_shared_from_this<CustomTaskGroup> {
public:
  // todo ad suspend
  CustomTaskGroup(ITaskHolder &taskHolder, std::shared_ptr<CustomTask> creator)
      : taskHolder(taskHolder), creator(creator){};
  void run(std::vector<int> &state, int job) {
    refCounter++;
    taskHolder.addTask(
        std::make_shared<CustomTask>(state, job, shared_from_this()));
  }
  bool wait(int continueAt) { // has to return in code if wait is true
                              // unregisterChild is responsible for adding the
                              // task again if it is ready
    creator->continueAt = continueAt;
    return refCounter > 0;
  }

  void unregisterChild() {
    int old_value;
    int new_value;
    do {
      old_value = refCounter.load();
      new_value = old_value - 1;
    } while (!refCounter.compare_exchange_weak(old_value, new_value));
    if (new_value == 0) {
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
};