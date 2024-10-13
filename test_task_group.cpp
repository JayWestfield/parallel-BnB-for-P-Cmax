#include <mutex>
#include <oneapi/tbb/task.h>
#include <shared_mutex>
#include <tbb/tbb.h>
#include <vector>
#include <tuple>
#include <iostream>

class TaskManager {
public:
    using SuspendedTask = tbb::task::suspend_point;
    using TaskTuple = std::tuple<int, std::vector<SuspendedTask>>;
    std::vector<TaskTuple> task_tuples;
    std::shared_mutex mtx;

    void addOrSuspendTask(int param) {
        {
            std::unique_lock lock(mtx);
            for (auto& task_tuple : task_tuples) {
                if (std::get<0>(task_tuple) == param) {
                    lock.unlock();
                    tbb::task::suspend([&, param](auto suspend_point) {
                        std::unique_lock inner_lock(mtx);
                        for (auto& inner_task_tuple : task_tuples) {
                            if (std::get<0>(inner_task_tuple) == param) {
                                std::get<1>(inner_task_tuple).push_back(suspend_point);
                                return;
                            }
                        }
                        tbb::task::resume(suspend_point);
                    });
                    return;
                }
            }
            task_tuples.push_back(std::make_tuple(param, std::vector<SuspendedTask>()));
        }
    }

    void resumeTasks(int param) {
        std::unique_lock lock(mtx);
        for (auto& task_tuple : task_tuples) {
            if (std::get<0>(task_tuple) == param) {
                for (auto& suspend_point : std::get<1>(task_tuple)) {
                    tbb::task::resume(suspend_point);
                }
                std::get<1>(task_tuple).clear();
                std::get<0>(task_tuple) = 0;
                break;
            }
        }
    }
};

// Beispiel-Task-Funktion
void exampleTask(TaskManager& manager, int param) {
    if (param > 10) return;
    manager.addOrSuspendTask(param);
     if (param > 8) std::cout << "Executing task with param: " << param << "\n";
    tbb::task_group tg;

    tg.run([&] { exampleTask(manager, param + 1); });
    tg.run([&] { exampleTask(manager, param + 1); });
    tg.wait();
    std::cout << "returning task with param: " << param << "\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    manager.resumeTasks(param);
}

int main() {
    TaskManager manager;
    tbb::task_group tg;
    tg.run([&] { exampleTask(manager, 1); });
    tg.run([&] { exampleTask(manager, 1); });
    tg.wait();
    return 0;
}
