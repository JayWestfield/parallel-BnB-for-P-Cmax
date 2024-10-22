
#include <atomic>
#include <vector>
struct customTask {
        customTask(std::vector<int> *state, int job, std::atomic<int> &parentCounter): state(state), job(job), parentCounter(parentCounter) {}
        std::vector<int> *state;
        int job;
        std::atomic<int> &parentCounter;
};
