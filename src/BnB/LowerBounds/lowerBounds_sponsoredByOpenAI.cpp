#include <vector>
#include <queue>
#include <algorithm>
#include <numeric>
#include <climits>
#include <iostream>
#include <atomic>
#include "lowerBounds.h"

// !!!! this was the first try but Chat Gpt provided a bad ( i am not even sure wether it is correct) implementation
class lowerBounds_sponsoredByOpenAI : public LowerBound_base {
public:
    // Constructor
    lowerBounds_sponsoredByOpenAI(std::atomic<bool> &foundOptimal, std::atomic<bool> &cancel)
        : LowerBound_base(foundOptimal, cancel), optimalCost(INT_MAX) {}

    // Function to solve the scheduling problem optimally
    int solve(const std::vector<int>& jobs, int numMachines) override {
        this->jobs = jobs;
        this->numMachines = numMachines;
        this->n = jobs.size();

        // Initialize priority queue with the root node
        std::vector<int> initialLoad(numMachines, 0);
        int upperBound = calculateUpperBound(initialLoad, 0);
        pq.push({initialLoad, 0, upperBound});

        // Branch and Bound process
        while (!pq.empty() && !terminate()) {
            Node current = pq.top();
            pq.pop();

            if (current.level == n) {
                optimalCost = std::min(optimalCost, current.upperBound);
                foundOptimal.store(true);
                continue;
            }

            for (int i = 0; i < numMachines; ++i) {
                std::vector<int> childLoad = current.load;
                childLoad[i] += jobs[current.level];
                int newUpperBound = calculateUpperBound(childLoad, current.level + 1);

                // Pruning: only consider this child if it improves the optimal cost
                if (newUpperBound < optimalCost) {
                    pq.push({childLoad, current.level + 1, newUpperBound});
                }
            }
        }

        return optimalCost;
    }

private:
    struct Node {
        std::vector<int> load;
        int level;
        int upperBound;

        bool operator<(const Node& other) const {
            return upperBound > other.upperBound;  // Priority queue based on lower upper bound
        }
    };

    std::vector<int> jobs;
    int numMachines;
    int n;
    int optimalCost;
    std::priority_queue<Node> pq;

    // Function to calculate the upper bound for the given loads
    int calculateUpperBound(const std::vector<int>& load, int level) {
        std::vector<int> remainingJobs(jobs.begin() + level, jobs.end());
        std::vector<int> newLoad = load;
        std::sort(remainingJobs.rbegin(), remainingJobs.rend());

        for (int job : remainingJobs) {
            *std::min_element(newLoad.begin(), newLoad.end()) += job;
        }

        return *std::max_element(newLoad.begin(), newLoad.end());
    }

    // Function to calculate the lower bound for the given loads
    int calculateLowerBound(const std::vector<int>& load, int level) {
        int remainingJobsSum = std::accumulate(jobs.begin() + level, jobs.end(), 0);
        int minLoad = *std::min_element(load.begin(), load.end());
        return std::max(minLoad, (remainingJobsSum + std::accumulate(load.begin(), load.end(), 0)) / numMachines);
    }
};

int main() {
    std::vector<int> jobs = {2, 3, 7, 5, 9, 12, 4, 6};
    int numMachines = 3;
    std::atomic<bool> foundOptimal(false);
    std::atomic<bool> cancel(false);

    lowerBounds_sponsoredByOpenAI scheduler(foundOptimal, cancel);
    int result = scheduler.solve(jobs, numMachines);
    std::cout << "Optimal Makespan: " << result << std::endl;

    return 0;
}
