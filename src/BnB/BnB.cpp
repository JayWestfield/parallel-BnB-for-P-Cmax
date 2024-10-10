#include "BnB.h"
#include <algorithm>
#include <iostream>
#include <tbb/tbb.h>
#include "STImpl.cpp"
#include <iostream>
#include <vector>
#include <iomanip>
#include <future>
// !!!!!!!!!!!!!!!!!!!! not maintained this is the initial Version that is just here because i might have left some comments with ideas about what to improve !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Constructor
BnBSolver::BnBSolver(bool irrelevance = true, bool gist = true, bool fur = true)  {
    this->irrelevance = irrelevance;
    std::cout << gist << std::endl;
    this->gist = gist;
    this->fur = fur;
}

int lowerBound;
std::atomic<int> upperBound;
std::vector<int> jobDurations;
std::vector<std::vector<int>> RET;
std::atomic<int> offset = 0;
//TODOS gist/st, initial bounds parallel, RET Parallel to see better speadups
int BnBSolver::trivialLowerBound() {
    return std::max(std::max(jobDurations[0], jobDurations[numMachines - 1] + jobDurations[numMachines]),std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines);
}

bool BnBSolver::lpt(std::vector<int> state, int job) {
    for (long unsigned int i = job; i < jobDurations.size(); i++) {
        int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
        state[minLoadMachine] += jobDurations[i];
    }
    int makespan = *std::max_element(state.begin(), state.end());
    if (makespan < upperBound) {
        updateBound(makespan);
        return true;
    } return false;
}
int BnBSolver::lPTUpperBound() {
    std::vector<int> upper(numMachines, 0);

    for (long unsigned int i = 0; i < jobDurations.size(); i++) {
        int minLoadMachine = std::min_element(upper.begin(), upper.end()) - upper.begin();
        upper[minLoadMachine] += jobDurations[i];
    }
    return *std::max_element(upper.begin(), upper.end());

}
int BnBSolver::left(int i, int u) {
    return RET[i + 1][u];
}
int BnBSolver::right(int i, int u) {
    if (u + jobDurations[i] > upperBound) return 0;
    return RET[i + 1][u + jobDurations[i]];
}
void BnBSolver::fillRET() {
    RET = std::vector<std::vector<int>>(jobDurations.size(), std::vector<int>(upperBound + 1));   
    for (int u = 0; u < upperBound; u++) {
        if (upperBound - jobDurations[jobDurations.size() - 1]  < u) { // TODO Branchless
            RET[jobDurations.size() - 1][u] = 1;
        } else {
            RET[jobDurations.size() - 1][u] = 2;
        }
    }
    for (long unsigned int i = 0; i < jobDurations.size(); i++) {
        RET[i][upperBound] = 1;
    }
    for (int i = jobDurations.size() - 2; i >= 0; i--) {
        for (int u = upperBound - 1; u >= 0; u --) {
            if (left(i, u) == left(i,u + 1) && right(i, u) == right(i,u + 1)) { // TODO Check wether branchless is worth it
                RET[i][u] = RET[i][u + 1];
            } else {
                RET[i][u] = RET[i][u + 1] + 1;
            }
        }
    }
    /*
    for (int i = 0; i < upperBound; i++) {
        if (RET[6][i] == RET[6][i + 1]) {
        std::cout << i  << " " << RET[6][i] << std::endl;
        }
    }*/
}


// do not consider Elements that are irreleveant to the instance according to Theorem 2
int BnBSolver::computeIrrelevanceIndex() { // this this will only be usefull with a better lower bound
    std::vector<int> sum(jobDurations.size() + 1);
    sum[0] = 0;

    std::partial_sum(jobDurations.begin(), jobDurations.end(), sum.begin() + 1);
    int i = jobDurations.size() - 1;
    // TODO think: do i need to update the lowerBound then aswell? no i dont think so
    while ( i > 0 && sum[i] < numMachines * (lowerBound - jobDurations[i] + 1))  { // TODO think about that do i still need to add them in the end right?
        i--;
    }

    return i;
}

// Solve the problem using Branch and Bound
void BnBSolver::init() {
    upperBound = lPTUpperBound();
    lowerBound = trivialLowerBound(); // TODO better lower bopnding techniques
    lastRelevantJobIndex = jobDurations.size() - 1;
    offset = 0;
    if (irrelevance) lastRelevantJobIndex = computeIrrelevanceIndex();

    //std::cout << "Initial Upper Bound: " << upperBound  << " Initial lower Bound: "  << lowerBound << std::endl;
    initialUpperBound = upperBound;

    fillRET();
    STInstance = std::make_unique<STImpl>( jobDurations.size(), offset, &RET); // TODO create after irrelevance index

    // set index for one  relevant JobSize Left 
    lastJobSize = lastRelevantJobIndex;
    int i = lastRelevantJobIndex;
    int lastJoblength = jobDurations[i];
    while (jobDurations[--i] == lastJoblength && i > 0) {}
    lastJobSize = i + 1;
}

int BnBSolver::solve(int numMachine, const std::vector<int>& jobDuration) {
    visitedNodes = 0;
    numMachines = numMachine;
    jobDurations = jobDuration;

    std::sort(jobDurations.begin(), jobDurations.end(), std::greater<int>()); // are instances big enough to sort in parallel? because for 20 items it is not worth to sort them in parallel"

    init(); 

    std::vector<int> initialState(numMachines, 0);

    solveInstance(initialState, 0);
    arena.execute([&]() { /* This ensures that all tasks are completed */ });
    return upperBound;
}
void BnBSolver::updateBound(int newBound) {
    //std::cout << "Bound update:" << upperBound << " -> " << newBound << std::endl;
    std::unique_lock lock(mutex);
    if (newBound >= upperBound) return;
    upperBound = newBound;
    offset = initialUpperBound - newBound; // TODO Fix sth wwith segmentation fault occurs somewhere
    /*int currentUpperBound = upperBound.load();
    while (newBound < currentUpperBound && 
        !upperBound.compare_exchange_weak(currentUpperBound, newBound)) { // TODO offsetupdate
    } 
    int currentOffset = offset.load(); // it might be that retlookups see a not yet updated version of the offset (not sure wether that can be a problem)
    while (newOffset > currentOffset && 
        !offset.compare_exchange_weak(currentOffset, newOffset)) { // TODO offsetupdate
    }*/
    //TODO find a good way to clear the map wihtout interfering with accesses of other threads
    if(gist) STInstance->boundUpdate(offset);
}

bool BnBSolver::lookupRet(int i, int j, int job) {
    std::shared_lock lock(mutex);
    const int off = offset; // !! i am not 100% sure wether the compiler could optimizes that to allow race conditions
    //std::cout << i << " " << j  << " " << off << std::endl;
    return RET[job][i + off] == RET[job][j + off];
}
bool BnBSolver::lookupRetFur(int i, int j, int job) { // this is awful codestyle needs to be refactored
    std::shared_lock lock(mutex);
    return RET[job][i + (initialUpperBound - upperBound)] == RET[job][upperBound - j]; // need to make sure that upperbound and offset are correct done with initialBound (suboptimal)
}

bool volatile BnBSolver::solveInstance(std::vector<int> state, int job) { //limits || visitedNodes > 15000000
    std::sort(state.begin(), state.end()); // currently for simplification
    // TODO add the gist first previously and in the find enqueu it in the arena
    if (upperBound == lowerBound ) return false; // works better if the lower bound gets better abort for long runs because it takes quite long some times
   
    int makespan = state[state.size() - 1];
    visitedNodes++; // this is not 100% accurate atm only tracks the solve Instancve calls, but f.e. Rule 5 counts as one visited node

    if (visitedNodes % 100000000 == 0) {
        std::cout.precision(5);
        std::cout << "visited nodes: " <<  std::scientific << visitedNodes  << " current Bound: " << upperBound << " " << job << " makespan " << makespan  << std:: endl;
        std::cout << "current state: ";
        for (int i : state) std::cout << " " << i;
        std::cout << std::endl;
    }

    if (job > lastRelevantJobIndex) {
        if (makespan < upperBound) {
            updateBound(makespan);
            return true;
        } else return false;
    } else if (makespan >= upperBound) return false; //clearly infeasible


    if (job == lastRelevantJobIndex - 2) { // 3 jobs remaining
        std::sort(state.begin(), state.end()); // for the first try
        std::vector<int> one = state;
        bool first = lpt(one, job);
        std::vector<int> two = state;
        two[1] += jobDurations[job++];
        bool second = lpt(two, job);
        return first || second;
    } 
    
    else if (job >= lastJobSize) { // Rule 5
        // TODO it should be able to do this faster (at least for many jobs with the same size) by keeping the state in a priority queu or sth like that
        for (long unsigned int i = job; i < jobDurations.size(); i++) {
            int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
            state[minLoadMachine] += jobDurations[i];
        }
        makespan = *std::max_element(state.begin(), state.end());
        if (makespan < upperBound) {
            //std::cout << "new Bound " << makespan << "old " << upperBound << std::endl;
            updateBound(makespan);
            return true;
        } else return false;
    } 
    std::cout << gist;
    if (gist )  { // TODO the gist checks were in the front check which is better
        
        auto exists = STInstance->exists(state, job);
        std::cout << exists << std::endl;
        if (exists == 2) return false;
        else if (exists == 1) {
            std::cout << "test" << std::endl;
            std::promise<bool> promise;
            auto future = promise.get_future();
            arena.enqueue([=, &promise]  {
                bool result = solveInstance(state, job);
                promise.set_value(result);
            });
            return future.get();  
        } //else STInstance->addPreviously(state, job);
    }
    if(fur) {
        for (int i = (state.size() - 1); i >= 0; i--) { // FUR
            if (state[i] + jobDurations[job] < upperBound && lookupRetFur(state[i], jobDurations[job], job)) {
                std::vector<int> next = state; // when implemented carefully there is no copy needed (but we are not that far yet)
                next[i] += jobDurations[job];
                if (solveInstance(next, job + 1)) {
                    if (state[i] + jobDurations[job] <= upperBound) { // we need to check for smaller or equal because if a better upper bound was found (by another thread) we still need to re solvethis
                        continue; // no need to check everything again only the clearly infeasible rule could theoretically be true but the others don't change
                    }
                } else if (state[i] + jobDurations[job] > upperBound) { // has to be handled, because another thread could find a new upperBound that makes this assignment invalid
                    continue;
                } else return false;
            }
        }
    }

    //TODO Rule 2 carefull because of job ordering etc
    //TODO Benchmarks might not be that interesting at first
    // first trivial recursion
    // TODO better Recursion (Rule 2 is missing i think and we should directly ignore the ones at or above the upper bound)



    bool foundBetterSolution = false;
    tbb::task_group tg;
    int endState = state.size();
    std::vector<int> repeat;
    if (numMachines > lastRelevantJobIndex - job + 1 ) endState = lastRelevantJobIndex - job + 1; // Rule 4 only usefull for more than 3 states otherwise rule 3 gets triggered
    for (int i = 0; i < endState; i++) {
        if (( i > 0 && state[i] == state[i - 1]) || state[i] + jobDurations[job] >= upperBound) continue; // Rule 1 + check directly wether the new state would be feasible
        if ( i > 0 && lookupRet(state[i], state[i - 1], job)) { //Rule 6
            repeat.push_back(i);
            continue;
        }
        std::vector<int> next = state;
        next[i] += jobDurations[job];
        tg.run([=, &tg, &foundBetterSolution] {
                    if (solveInstance(next, job + 1)) {
                        foundBetterSolution = true; // Update the flag if any task found a better solution
                    }
                });  
    }

    tg.wait();
    for (int i : repeat) {
        if (!lookupRet(state[i], state[i - 1], job)) { //Rule 6
           //std::cout << "repeat " << job << " " << i << " ";
            std::vector<int> next = state;
            next[i] += jobDurations[job];
            if (solveInstance(next, job + 1)) { // TODO parallel?
                        foundBetterSolution = true; // Update the flag if any task found a better solution
                    }
        }
    }
    if (gist) STInstance->addGist(state, job);
    return foundBetterSolution; // needed 
}
