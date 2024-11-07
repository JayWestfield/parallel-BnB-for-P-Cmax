// IConcurrentHashMap.h
#ifndef IConcurrentHashMapCombined_H
#define IConcurrentHashMapCombined_H
#include <memory>
#include <vector>
#include <string>
#include "hashing/hashing.hpp"
#include <cassert>
#include <tbb/tbb.h>
#include "../../../structures/DelayedTasksList.hpp"
#include "../../../structures/STEntry.hpp"
#include "../../../structures/SegmentedStack.hpp"
#include "../../../threadLocal/threadLocal.h"

class IConcurrentHashMapCombined
{
public:
    IConcurrentHashMapCombined(std::vector<std::unique_ptr<SegmentedStack<>>>& Gist_storage): Gist_storage(Gist_storage){};

    virtual ~IConcurrentHashMapCombined() = default;
    
    virtual DelayedTasksList * insert(int *gist, bool value) = 0;
    virtual int find(int *key) = 0;
    virtual void clear() = 0;
    virtual bool tryAddDelayed(std::shared_ptr<CustomTask> &task, int *gist) = 0;
    STEntry *createSTEntry(int *gist, bool finished) {
        return Gist_storage[threadIndex]->push(gist, finished);
    }
    void deleteSTEntry() {
        Gist_storage[threadIndex]->pop();
    }
    private:
    std::vector<std::unique_ptr<SegmentedStack<>>>& Gist_storage;
};

#endif