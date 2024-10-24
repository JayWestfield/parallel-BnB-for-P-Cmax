#ifndef SuspendedTaskHolder_impl
#define SuspendedTaskHolder_impl
#include "CustomTask.hpp"
#include <list>
#include <mutex>
//erstmal einfaches suspend und dann scheun ob das geht
// hold suspended Tasks should support a lock free retrieve and add oder einfach einen zusätzlichen task holder der kann dan beim resumedSuspendedTasks stealing auch random accesesed werden das ist vlt besser
// wobei ich ein bulk insert unterstützen will busk insert  wenn das eine liste ist kann ich die einfach anhängen fertig
// wenn ich die suspended tasks aber wirklich in der ST speichere dann lieber das ganze file kopieren um benchmark mit der alten version vergelichen zu können
// für St dann mit segmented versuchen
class SuspendedTaskHolder {
public:
  SuspendedTaskHolder() : stealMutex(){}
  // TODO stealTasks should return an array of elements / array of pointers
  std::shared_ptr<CustomTask> stealTasks() {
    std::unique_lock lock(stealMutex);
    if (resumedSuspendedTasks.empty()) {
      return nullptr;
    }
    std::shared_ptr<CustomTask> front = resumedSuspendedTasks.front();
    resumedSuspendedTasks.pop_front();

    return front;
  }
  // this should only be used by the corresponding thread
  std::shared_ptr<CustomTask> getNextTask() {
    std::unique_lock lock(stealMutex);

    if (resumedSuspendedTasks.empty()) {
      return nullptr;
    }
    std::shared_ptr<CustomTask> back = resumedSuspendedTasks.back();
    resumedSuspendedTasks.pop_back();
    return back;
  }

  void addTask(std::shared_ptr<CustomTask> newTask) {
    std::unique_lock lock(stealMutex);
    resumedSuspendedTasks.push_back(std::move(newTask));
  }

private:
  // TODO currently this mutex locks the whole datastructure but i would rather
  // have that the produccer (corresponding thread) does not need to get thsi
  // mutex since he will have many many interactions with that queue
  std::mutex stealMutex;
  // std::atomic<std::atomic_uint16_t>
  // tbb has no  concurrent list only a queue but i need to pop on both sides
  std::list<std::shared_ptr<CustomTask>> resumedSuspendedTasks;
};
#endif