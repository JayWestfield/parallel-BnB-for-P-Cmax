#pragma once

#include "structures/CustomTask.hpp"
#include "structures/STEntry.hpp"

inline void addDelayed(STEntry &entry, std::shared_ptr<CustomTask> &task) {
    auto test = new DelayedTasksList(task, entry.taskList);
    entry.taskList = test;
}


inline bool isSTEntryFinished(STEntry &entry) {
    return entry.taskList == nullptr;
}
