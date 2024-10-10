// IConcurrentHashMap.h
#ifndef ISingleThreadedHashMap_H
#define ISingleThreadedHashMap_H
#include <vector>
#include <string>
#include "../../hashing/hashing.hpp"
#include <cassert>
#include <tbb/tbb.h>

class ISingleThreadedHashMap
{
public:
    virtual void insert(const std::vector<int> &key) = 0;
    virtual bool find(const std::vector<int> &key) = 0;
    virtual void iterate() const = 0;
    virtual void clear() = 0;
};

#endif