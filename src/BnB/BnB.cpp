#include "BnB.h"
#include <algorithm>
#include <iostream>
#include <tbb/tbb.h>

// Constructor
BnBSolver::BnBSolver()  {}

int lowerBound;
std::atomic<int> upperBound;
std::vector<int> jobDurations;
std::vector<std::vector<int>> RET;
std::atomic<int> offset = 0;
tbb::spin_mutex bestSolutionMutex;

int BnBSolver::trivialLowerBound() {
    return std::accumulate(jobDurations.begin(), jobDurations.end(), 0) / numMachines;
}

bool BnBSolver::lpt(std::vector<int> state, int job) {

    for (int i = job; i < jobDurations.size(); i++) {
        int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
        state[minLoadMachine] += jobDurations[i];
    }
    int makespan = *std::max_element(state.begin(), state.end());
    if (makespan < upperBound) {
        updateBound(makespan);
        return true;
    } return false;
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
    for (int i = 0; i < jobDurations.size(); i++) {
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
    }/*
    for (int i = 0; i < upperBound; i++) {
        if (RET[6][i] == RET[6][i + 1]) {
        std::cout << i  << " " << RET[6][i] << std::endl;
        }
    }*/
}

// do not consider Elements that are irreleveant to the instance according to Theorem 2
void BnBSolver::irrelevanceIndex() { // this this will only be usefull with a better lower bound
    std::vector<int> sum(jobDurations.size() + 1);
    sum[0] = 0;
    std::partial_sum(jobDurations.begin(), jobDurations.end(), sum.begin() + 1);
    int i = jobDurations.size() - 1;
    while (sum[i] < numMachines * (lowerBound - jobDurations[i] + 1))  {
        i--;
    }
    std::cout << "number of irrelevant jobs " << jobDurations.size() - i -1 << std::endl;
    jobDurations.resize(i + 1);
}

// Solve the problem using Branch and Bound
void BnBSolver::init() {
    upperBound = std::numeric_limits<int>::max();
    lowerBound = trivialLowerBound(); // TODO better lower bopnding techniques
    irrelevanceIndex();
    std::vector<int> upper(numMachines, 0);
    lpt(upper,0); // use multiple upper bound techniques in parallel
    std::cout << "Initial Upper Bound provided by lpt is: " << upperBound << std::endl;
    std::cout << "Initial lower trivial Bound is: " << lowerBound << std::endl;

    initialUpperBound = upperBound;
    fillRET();

    // set index for one  relevant JobSize Left 
    lastJobSize = jobDurations.size();
    int i = jobDurations.size() - 1;
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
    return upperBound;
}
void BnBSolver::updateBound(int newBound) {
    int currentUpperBound = upperBound.load();
    while (newBound < currentUpperBound && 
        !upperBound.compare_exchange_weak(currentUpperBound, newBound)) { // TODO offsetupdate
    }
}
bool BnBSolver::lookupRet(int i, int j, int job) {
    const int off = offset; // !! i am not 100% sure wether the compiler could optimizes that to allow race conditions
    return RET[job][i + off] == RET[job][j + off];
}
bool BnBSolver::lookupRetFur(int i, int j, int job) { // this is awful codestyle needs to be refactored
    return RET[job][i + (initialUpperBound - upperBound)] == RET[job][upperBound - j]; // need to make sure that upperbound and offset are correct done with initialBound (suboptimal)
}

bool BnBSolver::solveInstance(std::vector<int> state, int job) {
    if (upperBound == lowerBound) return false; // works better if the lower bound gets better
    visitedNodes++; // this is not 100% accurate atm only tracks the solve Instancve calls, but f.e. Rule 5 counts as one visited node
    if (visitedNodes % 10000000 == 0) {
        std::cout << "nodes: " << visitedNodes  << "current Bound: " << upperBound << std::endl;
    }
    int makespan = *std::max_element(state.begin(), state.end());
    if (job >= jobDurations.size()) {
        if (makespan < upperBound) {
            std::cout << "new Bound " << makespan << "old " << upperBound << std::endl;

            updateBound(makespan);
            return true;
        } else return false;
    } else if (makespan >= upperBound) return false; //clearly infeasible
    else if (job == jobDurations.size() - 4) { // 3 jobs remaining
        std::sort(state.begin(), state.end(), std::greater<int>()); // for the first try
        std::vector<int> one = state;
        bool first = lpt(one, job);
        std::vector<int> two = state;
        two[1] += jobDurations[job++];
        bool second = lpt(two, job);
        return first || second;
    }
    else if (job >= lastJobSize) { // Rule 5
        // TODO it should be able to do this faster (at least for many jobs with the same size) by keeping the state in a priority queu or sth like that
        for (int i = job; i < jobDurations.size(); i++) {
            int minLoadMachine = std::min_element(state.begin(), state.end()) - state.begin();
            state[minLoadMachine] += jobDurations[i];
        }
        makespan = *std::max_element(state.begin(), state.end());
        if (makespan < upperBound) {
            std::cout << "new Bound " << makespan << "old " << upperBound << std::endl;
            updateBound(makespan);
            return true;
        } else return false;
    } 
    std::sort(state.begin(), state.end()); // currently for simplification
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
    //TODO more Rules
    //TODO Benchmarks might not be that interesting at first
    // first trivial recursion
    // TODO better Recursion (Rule 2 is missing i think and we should directly ignore the ones at or above the upper bound)
    bool foundBetterSolution = false;
    tbb::task_group tg;
    int endState = state.size();
    if (state.size() > jobDurations.size() - job ) endState = jobDurations.size() - job; // Rule 4 only usefull for more than 3 states otherwise rule 3 gets triggered
    for (int i = 0; i < endState; i++) {
        if ( i > 0 && state[i] == state[i - 1]) continue; // Rule 1 (not Rule 6 for now because unsure see notes)
        
        std::vector<int> next = state;
        next[i] += jobDurations[job];
        tg.run([=, &tg, &foundBetterSolution] {
                    if (solveInstance(next, job + 1)) {
                        foundBetterSolution = true; // Update the flag if any task found a better solution
                    }
                });    
    }
    tg.wait();
    return foundBetterSolution; // needed 

}
