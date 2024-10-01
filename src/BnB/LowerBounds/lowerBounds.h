#ifndef LowerBound_H
#define LowerBound_H
#include <atomic>
#include <vector>
#include <tbb/tbb.h>
class LowerBound_base {
public:
    LowerBound_base(std::atomic<bool> &foundOptimal, std::atomic<bool> &cancel): foundOptimal(foundOptimal),cancel(cancel){};
    virtual int solve(const std::vector<int>& jobs, int numMachines) = 0;

protected:
    std::atomic<bool> &foundOptimal;
    std::atomic<bool> &cancel;

    inline bool terminate() {
        return foundOptimal || cancel;
    }
};

#endif