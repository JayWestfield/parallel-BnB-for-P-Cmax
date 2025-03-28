#ifndef ST_H
#define ST_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>
#include <cassert>
#include "../threadLocal/threadLocal.h"
#include "../structures/CustomTask.hpp"
#include "customBnB/ST/customSharedLock.hpp"
#include "hashmap/hashing/hashing.hpp"

class ST_custom
{
public:
    ST_custom(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : jobSize(jobSize), offset(offset), RET(RET), vec_size(vec_size), maximumRETIndex(RET[0].size())
    {
    }

    virtual ~ST_custom() = default;

    virtual void addGist(const std::vector<int> &gist, int job) = 0;
    virtual int exists(const std::vector<int> &, int job) = 0;
    virtual void addPreviously(const std::vector<int> &gist, int job) = 0;
    virtual void boundUpdate(int offset) = 0;
    virtual std::vector<int> computeGist(const std::vector<int> &state, int job) = 0;
    virtual void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) = 0;
    virtual void addDelayed(std::shared_ptr<CustomTask> tag) = 0;
    virtual void resumeAllDelayedTasks() = 0;
    virtual void clear() = 0;
    virtual void prepareBoundUpdate() = 0;
    virtual void cancelExecution() = 0;
    virtual void helpWhileLock(std::unique_lock<std::mutex> &lock) = 0;
    virtual void work() = 0;
protected:
    int jobSize;
    int offset; // no atomic because the bound Update is sequential
    const std::vector<std::vector<int>> &RET;
    std::size_t vec_size;
    int maximumRETIndex;
};

#endif // ST_H
