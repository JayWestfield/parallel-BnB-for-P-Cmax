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
        for (auto& pair : delayedMap) {
            for (auto& sp : pair.second) {
                tbb::task::resume(sp);
            }
        }
        delayedMap.clear();
    }
};
