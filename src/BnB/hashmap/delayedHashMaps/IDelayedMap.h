#include <vector>
#include <tbb/task.h>
#include <tbb/task_group.h>
#include "../../hashing/hashing.hpp"

class IDelayedmap {
   
public:
    virtual ~IDelayedmap() = default;

    virtual void insert(const std::vector<int>& key, tbb::task::suspend_point suspendPoint) = 0;
    virtual void resume(const std::vector<int>& key) = 0;
    virtual void resumeAll() = 0;
    virtual std::vector<std::vector<int>> getNonEmptyKeys() const = 0;
};
