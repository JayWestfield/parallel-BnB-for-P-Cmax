#include <iostream>
#include <unordered_map>
#include <vector>
#include <tbb/task.h>
#include <tbb/task_group.h>
#include "IDelayedMap.h"

class HashMapWrapper : public IDelayedmap {

private:
    // Die zugrundeliegende Hashmap
    std::unordered_map<std::vector<int>, std::vector<tbb::task::suspend_point>, VectorHasher> delayedMap;

public:
    // Einfügen eines Schlüssels und eines Suspend Points
    void insert(const std::vector<int>& key, tbb::task::suspend_point suspendPoint) override {
        delayedMap[key].push_back(suspendPoint);
        // resume(key);
                        // tbb::task::resume(suspendPoint);

    }

    // Resumieren der Suspend Points, die einem Schlüssel zugeordnet sind
    void resume(const std::vector<int>& key) override {
        auto it = delayedMap.find(key);
        if (it != delayedMap.end()) {
            for (auto& sp : it->second) {
                tbb::task::resume(sp);
            }
            delayedMap.erase(it);
        }
    }

    // Resumieren aller Suspend Points und Leeren der Hashmap
    void resumeAll() override {
        logAllTasks();

        for (auto& pair : delayedMap) {
            for (auto& sp : pair.second) {
                tbb::task::resume(sp);
            }
        }
        delayedMap.clear();
    }
    // Loggen aller Tasks in der delayedMap
    void logAllTasks()  {
        std::ostringstream oss;
        for (const auto& pair : delayedMap) {
            oss << "Key: [ ";
            for (const auto& keyPart : pair.first) {
                oss << keyPart << ", ";
            }
            oss << "] has " << pair.second.size() << " suspend points." << std::endl;
        }
        std::cout << oss.str();
    }

    std::vector<std::vector<int>> getNonEmptyKeys() const override {
        std::vector<std::vector<int>> keys;
        for (const auto& pair : delayedMap) {
            if (!pair.second.empty()) {
                keys.push_back(pair.first);
            }
        }
        return keys;
    }

};
