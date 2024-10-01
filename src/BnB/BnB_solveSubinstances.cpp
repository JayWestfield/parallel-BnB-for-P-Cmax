#include <vector>
#include <queue>
#include <algorithm>
#include <numeric>
#include <climits>
#include <iostream>
#include <atomic>

class BnB_solveSubinstances {
public:
    // Constructor
    BnB_solveSubinstances(const std::vector<int>& jobs, int numMachines, std::atomic<bool> &foundOptimal, std::atomic<bool> &cancel)
        : jobs(jobs), numMachines(numMachines), n(jobs.size()), optimalCost(INT_MAX), foundOptimal(foundOptimal),cancel(cancel) {}

    // Function to solve the scheduling problem optimally
    int solve() {

        // Initialize priority queue with the root node
        std::vector<int> initialLoad(numMachines, 0);
        int upperBound = calculateUpperBound(initialLoad, 0);
        pq.push({initialLoad, 0, upperBound});

        // Branch and Bound process
        while (!pq.empty() && (!foundOptimal.load() && !cancel)) {
            Node current = pq.top();
            pq.pop();

            if (current.level == n) {
                optimalCost = std::min(optimalCost, current.upperBound);
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
    std::atomic<bool> &foundOptimal;
    std::atomic<bool> &cancel;
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
};

