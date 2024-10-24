// IConcurrentHashMap.h
#ifndef IConcurrentHashMap_H
#define IConcurrentHashMap_H
#include <vector>
#include <string>
#include "hashing/hashing.hpp"
#include <cassert>
#include <tbb/tbb.h>

class IConcurrentHashMap
{
public:
    virtual ~IConcurrentHashMap() = default;
    
    virtual void insert(const std::vector<int> &key, bool value) = 0;
    virtual int find(const std::vector<int> &key) = 0;
    virtual void iterate() const = 0;
    virtual void clear() = 0;
};

#endif