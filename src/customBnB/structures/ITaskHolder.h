
#include "CustomTask.hpp"
#include <list>
#include <memory>
#include <mutex>
#include <tbb/tbb.h>

// to simplify first it is a list with one lock in the end i hope to only use a
// lock for the steal Tasks and provide some mechanism to dive the Task Holder
// into two parts one that can be stolen and one that is exclusive to the
// running thread
class ITaskHolder {
public:
  virtual std::shared_ptr<CustomTask> stealTasks() = 0;
  virtual std::shared_ptr<CustomTask> getNextTask() = 0;

  virtual void addTask(std::shared_ptr<CustomTask> newTask) = 0;
};
