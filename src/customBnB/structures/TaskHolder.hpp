#ifndef CustomTaskGroup_impl
#define CustomTaskGroup_impl
#include "CustomTask.hpp"
#include <list>
#include <memory>
#include <mutex>
#include <tbb/tbb.h>

// to simplify first it is a list with one lock in the end i hope to only use a
// lock for the steal Tasks and provide some mechanism to dive the Task Holder
// into two parts one that can be stolen and one that is exclusive to the
// running thread
class TaskHolder {
public:
  TaskHolder() : stealMutex(), work() {}
  // TODO stealTasks should return an array of elements / array of pointers
  std::shared_ptr<CustomTask> stealTasks() {
    std::unique_lock lock(stealMutex);
    if (work.empty()) {
      return nullptr;
    }
    std::shared_ptr<CustomTask> front = std::move(work.front());
    work.pop_front();

    return front;
  }
  // this should only be used by the corresponding thread
  std::shared_ptr<CustomTask> getNextTask() {
    std::unique_lock lock(stealMutex);

    if (work.empty()) {
      return nullptr;
    }
    std::shared_ptr<CustomTask> back = std::move(work.back());
    work.pop_back();

    return back;
  }

  void addTask(std::shared_ptr<CustomTask> newTask) {
    std::unique_lock lock(stealMutex);
    work.push_back(std::move(newTask));
  }

private:
  // TODO currently this mutex locks the whole datastructure but i would rather
  // have that the produccer (corresponding thread) does not need to get thsi
  // mutex since he will have many many interactions with that queue
  std::mutex stealMutex;
  // std::atomic<std::atomic_uint16_t>
  // tbb has no  concurrent list only a queue but i need to pop on both sides
  std::list<std::shared_ptr<CustomTask>> work;
};
#endif