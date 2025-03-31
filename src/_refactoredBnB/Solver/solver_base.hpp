#pragma once

#include "../Structs/Difficulty.hpp"
#include "../Structs/TaskContext.hpp"
#include "../Structs/memory_monitor.hpp"
#include "../external/task-based-workstealing/src/work_stealing_config.hpp"
#include "_refactoredBnB/ST/ST_combined.hpp"
#include "_refactoredBnB/Structs/globalValues.hpp"
#include "_refactoredBnB/Structs/structCollection.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numeric>
#include <sstream>
#include <vector>

// TODO STStorage # initializing
// TODO think about initializethreadLocalVector is it needed do i need to adapt
// the wss?
// TODO use makro for cleaner return?
// TODO outource some stuff into other files especially structs

template <typename Hashmap, Config SolverConfig> class solver_base {
public:
  std::atomic<uint64_t> visitedNodes = 0;

  Difficulty hardness = Difficulty::lptOpt;
  std::chrono::_V2::system_clock::time_point lastUpdate;
  std::vector<std::chrono::duration<double>> timeFrames;
  // skip depth if it is a number < 1 interpret is as only gist lookups/adds for
  // x * last size jobindex
  // if it is >= look at jobindex < lastsize - x
  solver_base(int maxAllowedParalellism, int initialHashmapSize = 2000,
              int skipDepth = 1, int GistStorageSize = 1024)
      : maxAllowedParalellism(maxAllowedParalellism),
        scheduler(
            maxAllowedParalellism, [this]() { this->idleFunction(); },
            [this](Task<TaskContext> *task) {
              return this->solvePartial(task);
            }),
        STInstance(1, RET, scheduler, initialHashmapSize, maxAllowedParalellism,
                   GistStorageSize),
        skipDepth(skipDepth) {}

  void cancelExecution() {
    scheduler.stop();
    cancel = true;
    continueExecution = false;
    STInstance.cancelExecution();
  }
  int solve(Instance instance) {

    ws::stateLength = instance.numMachines;
    ws::gistLength = instance.numMachines + 1;
    ws::wrappedGistLength = instance.numMachines + 1 +
                            (SolverConfig.optimizations.use_max_offset ? 1 : 0);
    ws::maxThreads = maxAllowedParalellism;
    jobDurations = instance.jobDurations;
    numMachines = instance.numMachines;

    // assert(std::is_sorted(jobDurations.begin(), jobDurations.end(),
    //                       std::greater<int>()));
    offset = 1;

    initialLowerBound = trivialLowerBound();
    lowerBound = initialLowerBound;

    // initialize bounds (trivial nothing fancy here)
    initialUpperBound = lPTUpperBound();
    upperBound = initialUpperBound - 1;

    if (initialUpperBound == lowerBound) {
      hardness = Difficulty::trivial;
      continueExecution = false;
      return initialUpperBound;
    }

    // irrelevance
    lastRelevantJobIndex = jobDurations.size() - 1;
    if (SolverConfig.optimizations.use_irrelevance)
      lastRelevantJobIndex = computeIrrelevanceIndex(lowerBound);

    // RET
    offset = 1;

    fillRET();

    // one JobSize left
    int i = lastRelevantJobIndex;
    // TODO last size can be done async
    if (jobDurations[i] == 0)
      --i;
    while (i > 0 && jobDurations[--i] == jobDurations[lastRelevantJobIndex]) {
    }
    lastSizeJobIndex = i + 1;
    if (SolverConfig.logging.logInitialBounds)
      std::cout << "Initial Upper Bound: " << upperBound
                << " Initial lower Bound: " << lowerBound << std::endl;
    memoryMonitor monitor(continueExecution, [this]() {
      std::unique_lock lock(this->boundLock);
      this->STInstance.evict();
    });
    lastUpdate = std::chrono::high_resolution_clock::now();
    // start Computing
    std::vector<int> initialState(numMachines, 0);
    auto start = std::chrono::high_resolution_clock::now();
    // TODO base Args
    scheduler.setBaseTaskArgs(initialState, 0);
    scheduler.runAndWait();
    timeFrames.push_back(
        (std::chrono::high_resolution_clock::now() - lastUpdate));
    if (cancel) {
      return 0;
    }
    continueExecution = false;
    monitor.notify();
    if (initialUpperBound == upperBound + 1) {
      hardness = Difficulty::lptOpt;
    } else if (upperBound + 1 == lowerBound) {
      if (lowerBound != initialLowerBound)
        hardness = Difficulty::lowerBoundOptimal2;
      else
        hardness = Difficulty::lowerBoundOptimal;
    } else {
      hardness = Difficulty::full;
    }
    return upperBound + 1;
  }

private:
  int skipDepth;

  // instance
  int maxAllowedParalellism;
  int numMachines;
  std::vector<int> jobDurations;

  // general
  std::atomic<bool> foundOptimal = false;
  std::atomic<bool> cancel = false;

  std::atomic<bool> continueExecution = true;

  // Bounds
  std::atomic<int> upperBound;
  int initialLowerBound;
  std::atomic<int> lowerBound;
  int initialUpperBound;
  std::mutex boundLock;

  // RET
  std::vector<std::vector<int>> RET = {{}};
  std::atomic<int> offset;

  // ST
  ST<Hashmap, SolverConfig.optimizations.use_fingerprint,
     SolverConfig.optimizations.use_max_offset,
     SolverConfig.logging.detailedLogging>
      STInstance;

  // only one jobsize left
  int lastSizeJobIndex;
  int lastRelevantJobIndex;

  wss<TaskContext> scheduler;

  // constexpr?
  bool inline skipLookup(int depth) {
    return depth >= lastSizeJobIndex - skipDepth;
  }
  bool solvePartial(Task<TaskContext> *task) {
    ws::initializeThreadLocalVector(numMachines);

    auto &[state, job, unsortedState, sameJobsize, continueAt, r6, loopIndex] =
        (*task).context;
    logging(state, job, "Work on: ");
    switch (continueAt) {
    case Continuation::InitialStart: {
      if (foundOptimal || cancel) {
        //    early return should be enough (but
        //   what if another task is suspended well we can just return the
        //   suspended one s manually)
        //   redundancyFinish();
        // task is already marked finihs if return true
        // scheduler.cancelTask(task);
        return true;
      }
      int makespan = state[numMachines - 1];

      int nodes = visitedNodes.fetch_add(1, std::memory_order_relaxed);
      if (SolverConfig.logging.logNodes && nodes % 100000 == 0) {
        std::cout << "visited nodes: " << visitedNodes
                  << " current Bound: " << upperBound << " " << job
                  << " makespan " << makespan << std::endl;
        std::cout << "current state: ";
        for (int i : state)
          std::cout << " " << i;
        std::cout << std::endl;
      }

      if (job > lastRelevantJobIndex) {
        updateBound(makespan);
        return true;
      } else if (makespan > upperBound) {
        return true;
      } // 3 jobs remaining
      else if (job == lastRelevantJobIndex - 2) {
        std::vector<int> one = state;
        lpt(one, job);
        state[1] += jobDurations[job];
        lpt(state, job + 1);
        return true;
      } else if (job >= lastSizeJobIndex) { // Rule 5
        lpt(state, job);
        return true;
      }
      continueAt = Continuation::STCHECK;
    }
    case Continuation::STCHECK:
      if (SolverConfig.optimizations.use_gists && !skipLookup(job)) {
        FindGistResult exists = STInstance.exists(state, job);
        switch (exists) {
        case FindGistResult::COMPLETED:
          logging(state, job, "gist found ");
          return true;
        case FindGistResult::STARTED:
          assert(SolverConfig.optimizations.use_add_previously &&
                 "Error cannot be found as started if we do not use "
                 "addPreviously");
          if (SolverConfig.optimizations.use_add_previously) {
            STInstance.addDelayed(task);
            return false;
          }
          break;
        case FindGistResult::NOT_FOUND:
          if (SolverConfig.optimizations.use_add_previously)
            STInstance.addPreviously(state, job);
        }
      }
      continueAt = Continuation::FUR;
    case Continuation::FUR: {
      // FUR
      if (SolverConfig.optimizations.use_fur) {
        int start = (numMachines - 1);
        if (loopIndex >= 0) {
          if (state[loopIndex] + jobDurations[job] <= upperBound &&
              lookupRetFur(state[loopIndex], jobDurations[job], job)) {
            if (SolverConfig.optimizations.use_gists && !skipLookup(job))
              STInstance.addGist(state, job);
            return true;
          } else {
            start = loopIndex - 1;
          }
          loopIndex = -1;
        }
        for (int i = start; i >= 0; i--) {
          if (state[i] + jobDurations[job] <= upperBound &&
              lookupRetFur(state[i], jobDurations[job], job)) {
            auto local = state;
            local[i] += jobDurations[job];
            ws::threadLocalStateVector = state;
            assert(ws::threadLocalStateVector == state);
            // might not need to copy here but carefully
            ws::threadLocalStateVector[i] += jobDurations[job];
            resortAfterIncrement(local, i);
            loopIndex = i;
            scheduler.addChild(task, local, job + 1);
            scheduler.waitTask(task);
            return false;
          }
        }
      }
      continueAt = Continuation::BASECASE;
    }

    case Continuation::BASECASE: {
      int endState = numMachines;
      // index of assignments that are currently ignored by Rule 6 (same Ret
      // entry than the previous job and can therefore be ignored)

      // index of assignments, that would lead to a gist that is already
      // handled somewhere else Rule 4
      if (numMachines > lastRelevantJobIndex - job + 1)
        endState = lastRelevantJobIndex - job + 1;
      assert(endState <= numMachines);

      // same job size rule
      if (sameJobsize >= 0) {
        assert(sameJobsize < numMachines);
        for (int l = 0; l <= sameJobsize; l++) {
          if (unsortedState[l] + jobDurations[job] > upperBound)
            continue;
          // Rule 1
          bool R1_pruned = false;
          for (int a = l + 1; a <= sameJobsize; a++) {
            R1_pruned |= unsortedState[a] == unsortedState[l];
          }
          if (R1_pruned)
            continue;
          // TODO in the end rule 6 check about the unsorted state not the
          // sorted if sameJobSize >= 0
          // bool R6_pruned = false;
          // need to run the last one as a task because the same job size rule
          // only says the other assignment is done somewhere but wwe have no
          // guarantee that this is finished before this here so it might lead
          // to a bound update after the continuation:Delayed happened !!!
          // for (int a = l + 1; a <= sameJobsize; a++) {
          //   if (lookupRet(unsortedState[l], unsortedState[a], job)) {
          //     R6_pruned = true;
          //     r6.push_back(l);
          //     break;
          //   }
          // }
          // if (R6_pruned)
          //   continue;

          // find the corresponding index in the sorted state ( dont care for
          // same load machines those are filtered out above)
          int i = -1;
          for (int a = numMachines - 1; a >= 0; a--) {
            if (state[a] == unsortedState[l]) {
              i = a;
              break;
            }
          }
          // why do i check the ret twice for rule 6?
          assert(i >= 0 && i < numMachines);
          // i think rule 6 here is buggy we cannot ensure that the other (i+1)
          // is assigned in this task but if it is done in another task and that
          // yields a better solution, but this task here finishes earlier we
          // wil not retry this assignment because we thought this is the same
          // if (i < sameJobsize &&
          //     lookupRet(state[i], state[i + 1], job)) { // Rule 6
          //   r6.push_back(i);
          //   continue;
          // }
          ws::threadLocalStateVector = state;
          assert(ws::threadLocalStateVector.size() ==
                 static_cast<size_t>(numMachines));
          ws::threadLocalStateVector[i] += jobDurations[job];
          assert(ws::threadLocalStateVector.size() ==
                 static_cast<size_t>(numMachines));

          resortAfterIncrement(ws::threadLocalStateVector, i);
          if (jobDurations[job] == jobDurations[job + 1]) {
            std::vector<int> unsorted = unsortedState;
            unsorted[l] += jobDurations[job];
            if (SolverConfig.optimizations.use_gists && !skipLookup(job + 1)) {
              FindGistResult ex =
                  STInstance.exists(ws::threadLocalStateVector, job + 1);
              if (ex != FindGistResult::COMPLETED)
                scheduler.addChild(task, ws::threadLocalStateVector, job + 1,
                                   unsorted, l);
              // TODO for FindGistResult::STARTED the task could theoretically
              // be spawned but not scheduled but the wss does not support
              // that
            } else {
              scheduler.addChild(task, ws::threadLocalStateVector, job + 1,
                                 unsorted, l);
            }
          } else {
            if (SolverConfig.optimizations.use_gists && !skipLookup(job + 1)) {
              FindGistResult ex =
                  STInstance.exists(ws::threadLocalStateVector, job + 1);
              if (ex != FindGistResult::COMPLETED)
                scheduler.addChild(task, ws::threadLocalStateVector, job + 1);
              // TODO for FindGistResult::STARTED the task could theoretically
              // be spawned but not scheduled but the wss does not support
              // that
            } else {
              scheduler.addChild(task, ws::threadLocalStateVector, job + 1);
            }
          }
        }
      } else {
        // base case
        for (int i = 0; i < endState; i++) {
          if ((i > endState - 1 && state[i] == state[i + 1]) ||
              state[i] + jobDurations[job] > upperBound)
            continue; // Rule 1 + check directly wether the new state would be
          // feasible
          if (i > endState - 1 &&
              lookupRet(state[i], state[i + 1], job)) { // Rule 6
            r6.push_back(i);
            continue;
          }
          ws::threadLocalStateVector = state;
          assert(ws::threadLocalStateVector.size() ==
                 static_cast<size_t>(numMachines));
          ws::threadLocalStateVector[i] += jobDurations[job];
          assert(ws::threadLocalStateVector.size() ==
                 static_cast<size_t>(numMachines));

          resortAfterIncrement(ws::threadLocalStateVector, i);
          if (jobDurations[job] == jobDurations[job + 1]) {
            std::vector<int> unsorted = state;
            unsorted[i] += jobDurations[job];
            if (SolverConfig.optimizations.use_gists && !skipLookup(job + 1)) {
              FindGistResult ex =
                  STInstance.exists(ws::threadLocalStateVector, job + 1);
              if (ex != FindGistResult::COMPLETED)
                scheduler.addChild(task, ws::threadLocalStateVector, job + 1,
                                   unsorted, i);
              // TODO for FindGistResult::STARTED the task could theoretically
              // be spawned but not scheduled but the wss does not support that
            } else {
              scheduler.addChild(task, ws::threadLocalStateVector, job + 1,
                                 unsorted, i);
            }
          } else {
            if (SolverConfig.optimizations.use_gists && !skipLookup(job + 1)) {
              FindGistResult ex =
                  STInstance.exists(ws::threadLocalStateVector, job + 1);
              if (ex != FindGistResult::COMPLETED)
                scheduler.addChild(task, ws::threadLocalStateVector, job + 1);
              // TODO for FindGistResult::STARTED the task could theoretically
              // be spawned but not scheduled but the wss does not support that
            } else {
              scheduler.addChild(task, ws::threadLocalStateVector, job + 1);
            }
          }
        }
      }
    }
      continueAt = Continuation::DELAYED;
      scheduler.waitTask(task);
      return false;
    case Continuation::DELAYED:
      if (!r6.empty()) {
        for (int i : r6) {
          assert(i < numMachines - 1);
          if (!lookupRet(state[i], state[i + 1], job)) { // Rule 6
            ws::threadLocalStateVector = state;
            ws::threadLocalStateVector[i] += jobDurations[job];
            resortAfterIncrement(ws::threadLocalStateVector, i);
            scheduler.addChild(task, ws::threadLocalStateVector, job + 1);
          }
        }
        continueAt = Continuation::END;
        scheduler.waitTask(task);
        return false;
      }

    case Continuation::END:
      if (job == 0) {
        foundOptimal = true; // to ensure ending
      }
      if (SolverConfig.optimizations.use_gists)
        STInstance.addGist(state, job);
      return true;
    }
    throw std::invalid_argument(
        "received invalid ContinuationToken or missing return");
    return true;
  }
  void inline idleFunction() {
    if (SolverConfig.optimizations.use_gists)
      STInstance.work();
  }
  inline int trivialLowerBound() {
    return std::max(
        std::max(jobDurations[0],
                 jobDurations[numMachines - 1] + jobDurations[numMachines]),
        ((int)std::ceil(((double)std::accumulate(jobDurations.begin(),
                                                 jobDurations.end(), 0)) /
                        (double)numMachines)));
  }

  /**
   * @brief find index, to when jobs are no longer relevant regarding the
   * given Bound.
   */
  int computeIrrelevanceIndex(int bound) {
    std::vector<int> sum(jobDurations.size() + 1);
    sum[0] = 0;

    std::partial_sum(jobDurations.begin(), jobDurations.end(), sum.begin() + 1);
    int i = jobDurations.size() - 1;
    while (i > 0 && sum[i] < numMachines * (lowerBound - jobDurations[i] + 1)) {
      i--;
    }

    return i;
  }
  inline bool lookupRet(int i, int j, int job) {
    const int off = offset.load();
    if (std::max(i, j) + off >= (int)RET[job].size())
      return false;
    assert(i + off < (int)RET[job].size() && j + off < (int)RET[job].size());
    return RET[job][i + off] == RET[job][j + off];
  }
  inline bool lookupRetFur(int i, int j, int job) {
    const int off = offset.load();
    if (i + off >= (int)RET[job].size())
      return false;
    assert(i + off < (int)RET[job].size() &&
           initialUpperBound - j < (int)RET[job].size());
    return RET[job][i + off] == RET[job][initialUpperBound - j];
  }

  /**
   * @brief runs Lpt on the given partialassignment
   */
  void lpt(std::vector<int> &state, int job) {
    for (long unsigned int i = job; i < jobDurations.size(); i++) {
      int minLoadMachine =
          std::min_element(state.begin(), state.end()) - state.begin();
      assert(minLoadMachine >= 0 && minLoadMachine < numMachines);
      state[minLoadMachine] += jobDurations[i];
    }
    int makespan = *std::max_element(state.begin(), state.end());
    if (makespan <= upperBound) {
      updateBound(makespan);
    }
    return;
  }

  /**
   * @brief compute an upper bound via LPT heuristic
   */
  int lPTUpperBound() {
    std::vector<int> upper(numMachines, 0);
    for (long unsigned int i = 0; i < jobDurations.size(); i++) {
      int minLoadMachine =
          std::min_element(upper.begin(), upper.end()) - upper.begin();
      assert(minLoadMachine >= 0 && (std::size_t)minLoadMachine < upper.size());
      upper[minLoadMachine] += jobDurations[i];
    }
    return *std::max_element(upper.begin(), upper.end());
  }
  /**
   * @brief initializes and fills the RET
   */
  void fillRET() {
    auto left = [this](int i, int u) { return RET[i + 1][u]; };
    auto right = [this](int i, int u) {
      if (u + jobDurations[i] > initialUpperBound)
        return 0;
      return RET[i + 1][u + jobDurations[i]];
    };
    RET = std::vector<std::vector<int>>(
        lastRelevantJobIndex + 1, std::vector<int>(initialUpperBound + 1));
#pragma omp unroll 8
    for (auto u = 0; u <= initialUpperBound;
         u++) { // TODO there is some point u where everithing before is a 1
                // and after it is a 2 so 2 for loops could be less branches
      if (u + jobDurations[lastRelevantJobIndex] > initialUpperBound) {
        RET[lastRelevantJobIndex][u] = 1;
      } else {
        RET[lastRelevantJobIndex][u] = 2;
      }
    }
#pragma omp unroll 8
    for (auto i = 0; i <= lastRelevantJobIndex; i++) {
      RET[i][initialUpperBound] = 1;
    }
    for (auto i = lastRelevantJobIndex - 1; i >= 0; i--) {
      for (int u = initialUpperBound - 1; u >= 0; u--) {
        if (left(i, u) == left(i, u + 1) && right(i, u) == right(i, u + 1)) {
          RET[i][u] = RET[i][u + 1];
        } else {
          RET[i][u] = RET[i][u + 1] + 1;
        }
      }
    }
  }
  inline void resortAfterIncrement(std::vector<int> &vec, size_t index) {
    assert(index < vec.size());
    int incrementedValue = vec[index];
    // since it was an increment the newPosIt can only be at the same index or
    // after
    auto newPosIt =
        std::upper_bound(vec.begin() + index, vec.end(), incrementedValue);
    assert(ws::stateLength == vec.size());
    size_t newPosIndex = std::distance(vec.begin(), newPosIt);
    if (newPosIndex == index) {
      return;
    }
    std::rotate(vec.begin() + index, vec.begin() + index + 1,
                vec.begin() + newPosIndex);
    // assert(std::is_sorted(vec.begin(), vec.end()));
  }

  /**
   * @brief tightens the upper bound and updates the offset for the RET as
   * well as updating the ST and restarting necessary tasks
   */
  void updateBound(int newBound) {
    assert(newBound >= lowerBound);
    if (newBound == lowerBound) {
      foundOptimal = true;
      STInstance.cancelExecution();
    }

    if (SolverConfig.logging.logBound)
      std::cout << "try Bound " << newBound << lowerBound << std::endl;

    if (newBound > upperBound)
      return;
    if (SolverConfig.logging.logBound)
      std::cout << "new Bound " << newBound << std::endl;
    std::unique_lock lock(boundLock, std::try_to_lock);
    // this one does not appear often so no need to optimize that
    while (!lock.owns_lock()) {
      STInstance.helpWhileLock(lock);
    }
    if (newBound > upperBound)
      return;
    const int newOffset = initialUpperBound - (newBound - 1);
    if (SolverConfig.optimizations.use_gists) {
      STInstance.prepareBoundUpdate();
    }
    upperBound.store(newBound - 1);
    // no need to migrate hashtable/resume if new bound is optimal
    if (newBound == lowerBound) {
      return;
    }

    offset.store(newOffset);
    // it is importatnt to d othe bound update on the ST before updating the
    // offset/upperBound, because otherwise the exist might return a false
    // positive (i am not 100% sure why)
    if (SolverConfig.optimizations.use_gists) {
      STInstance.boundUpdate(newOffset);
    }
    upperBound.store(newBound - 1);
    offset.store(newOffset);

    auto newTime = std::chrono::high_resolution_clock::now();
    timeFrames.push_back((std::chrono::duration<double>)(newTime - lastUpdate));
    lastUpdate = newTime;
    if (SolverConfig.logging.logBound)
      std::cout << "new Bound " << newBound << "finished" << std::endl;
  }

  template <typename T>
  void logging(const std::vector<int> &state, int job, T message = "") {
    if (!SolverConfig.logging.detailedLogging)
      return;
    std::stringstream gis;
    gis << message << " ";
    for (auto vla : state)
      gis << vla << ", ";
    gis << " Job: " << job << std::endl;
    std::cout << gis.str();
  }
};