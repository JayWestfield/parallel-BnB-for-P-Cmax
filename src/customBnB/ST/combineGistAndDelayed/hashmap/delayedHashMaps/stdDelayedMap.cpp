#include "../../../../structures/SuspendedTaskHolder.hpp"
#include "IDelayedMap.h"
#include <tbb/task.h>
#include <tbb/task_group.h>
#include <unordered_map>
#include <vector>
class HashMapWrapper : public IDelayedmap {

private:
  // Die zugrundeliegende Hashmap
  std::unordered_map<std::vector<int>, std::vector<std::shared_ptr<CustomTask>>,
                     hashing::VectorHasher>
      delayedMap;
  bool disabled = false;
  ITaskHolder &suspendedTasks;

public:
  HashMapWrapper(ITaskHolder &suspendedTasks)
      : suspendedTasks(suspendedTasks) {}
  // Einfügen eines Schlüssels und eines Suspend Points
  void insert(const std::vector<int> &key,
              std::shared_ptr<CustomTask> task) override {
    if (disabled) {
      suspendedTasks.addTask(std::move(task));
    } else {
      delayedMap[key].push_back(std::move(task));
    }
  }

  // Resumieren der Suspend Points, die einem Schlüssel zugeordnet sind
  void resume(const std::vector<int> &key) override {
    auto it = delayedMap.find(key);
    if (it != delayedMap.end()) {
      for (auto sp : it->second) {
        suspendedTasks.addTask(std::move(sp));
      }
      delayedMap.erase(it);
    }
  }

  // Resumieren aller Suspend Points und Leeren der Hashmap
  void resumeAll() override {
    // logAllTasks();

    for (auto &pair : delayedMap) {
      for (auto &sp : pair.second) {
        suspendedTasks.addTask(std::move(sp));
      }
    }
    delayedMap.clear();
  }

  std::vector<std::vector<int>> getNonEmptyKeys() const override {
    std::vector<std::vector<int>> keys;
    for (const auto &pair : delayedMap) {
      if (!pair.second.empty()) {
        keys.push_back(pair.first);
      }
    }
    return keys;
  }

  void cancelExecution() override {
    disabled = true;
    resumeAll();
  }
};
