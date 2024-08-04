#ifndef ST_H
#define ST_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>

struct VectorHasher {
    std::size_t hash(const std::vector<int>& vec) const {
        std::size_t seed = vec.size();
        for (auto& i : vec) {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
    bool equal(const std::vector<int>& a, const std::vector<int>& b) const {
        for (long unsigned int i = 0; i < a.size(); i++) if (a[i] != b[i]) return false;

        return true;
    }
};
class ST {
public:
    // Konstruktor mit Parameter int jobSize
    ST(int jobSize, std::atomic<int>* upperBound, std::atomic<int>* offset, std::vector<std::vector<int>>* RET,  std::shared_mutex* boundLock) : jobSize(jobSize),upperBound(upperBound), offset(offset), RET(RET)  {}

    virtual ~ST() = default;

    virtual void addGist(std::vector<int> gist, int job) = 0;
    virtual int exists(std::vector<int> gist, int job) = 0;
    virtual void addPreviously(std::vector<int> gist, int job) = 0;
    virtual void boundUpdate() = 0;
    virtual std::vector<int> computeGist(std::vector<int> state, int job) = 0;
    virtual void addDelayed(std::vector<int> gist, int job, oneapi::tbb::task::suspend_point tag) = 0;
    virtual void resumeAllDelayedTasks() = 0;
    virtual void clear() = 0;
protected:
    int jobSize; // Member-Variable zum Speichern der jobSize
    std::atomic<int>* upperBound;
    std::atomic<int>* offset;
    std::vector<std::vector<int>>* RET;
    std::shared_mutex boundLock;
};

#endif // ST_H
