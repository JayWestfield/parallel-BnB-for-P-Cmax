#ifndef ST_H
#define ST_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>
#include <cassert>
#include "threadLocal/threadLocal.h"
#include "hashing/hashFuntions.hpp"

class ST
{
public:
    // Konstruktor mit Parameter int jobSize
    ST(int jobSize, int offset, const std::vector<std::vector<int>> &RET, std::size_t vec_size) : jobSize(jobSize), offset(offset), RET(RET), vec_size(vec_size), maximumRETIndex(RET[0].size())
    {
    }

    virtual ~ST() = default;

    virtual void addGist(const std::vector<int> &gist, int job) = 0;
    virtual int exists(const std::vector<int> &, int job) = 0;
    virtual void addPreviously(const std::vector<int> &gist, int job) = 0;
    virtual void boundUpdate(int offset) = 0;
    virtual std::vector<int> computeGist(const std::vector<int> &state, int job) = 0;
    virtual void computeGist2(const std::vector<int> &state, int job, std::vector<int> &gist) = 0;
    virtual void addDelayed(const std::vector<int> &gist, int job, oneapi::tbb::task::suspend_point tag) = 0;
    virtual void resumeAllDelayedTasks() = 0;
    virtual void clear() = 0;
    virtual void prepareBoundUpdate() = 0;
    // ob das  funktioniert?
    // static thread_local std::vector<int> threadLocalVector;

protected:
    int jobSize; // Member-Variable zum Speichern der jobSize
    int offset;
    const std::vector<std::vector<int>> &RET;
    std::size_t vec_size;
    int maximumRETIndex;
};

#endif // ST_H
