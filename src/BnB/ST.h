#ifndef ST_H
#define ST_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>

struct VectorHasher
{
    std::size_t hash(const std::vector<int> &vec) const
    {
        std::size_t seed = vec.size();
        for (auto &i : vec)
        {
            seed ^= i + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
    bool equal(const std::vector<int> &a, const std::vector<int> &b) const
    {
        for (long unsigned int i = 0; i < a.size(); i++)
            if (a[i] != b[i])
                return false;

        return true;
    }
};
class ST
{
public:
    // Konstruktor mit Parameter int jobSize
    ST(int jobSize, int offset, std::vector<std::vector<int>> *RET) : jobSize(jobSize), offset(offset), RET(RET) {}

    virtual ~ST() = default;

    virtual void addGist(const std::vector<int> &gist, int job) = 0;
    virtual int exists(const std::vector<int> &, int job) = 0;
    virtual void addPreviously(const std::vector<int> &gist, int job) = 0;
    virtual void boundUpdate(int offset) = 0;
    virtual std::vector<int> computeGist(const std::vector<int> &state, int job) = 0;
    virtual void addDelayed(const std::vector<int> &gist, int job, oneapi::tbb::task::suspend_point tag) = 0;
    virtual void resumeAllDelayedTasks() = 0;
    virtual void clear() = 0;

protected:
    int jobSize; // Member-Variable zum Speichern der jobSize
    int offset;
    std::vector<std::vector<int>> *RET;
};

#endif // ST_H
