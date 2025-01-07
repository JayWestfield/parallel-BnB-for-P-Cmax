#ifndef BnB_base_Impl_H
#define BnB_base_Impl_H

#include "../BnB/BnB_base.h"
#include "../BnB/LowerBounds/lowerBounds_ret.cpp"
#include "CustomTaskGroup.hpp"
#include "ST/STImplSimplCustomLock.cpp"
#include "ST/combineGistAndDelayed/ST_combined.hpp"

#include "ST/combineGistAndDelayed/hashmap/hashing/hashing.hpp"
#include "structures/CustomTask.hpp"
#include "structures/FastRandom.hpp"
#include "structures/SuspendedTaskHolder.hpp"
#include "structures/TaskHolder.hpp"
#include "threadLocal/threadLocal.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>
const int limitedTaskSpawning = 2000;
extern int gistLength;
extern int wrappedGistLength;

class BnB_base_custom_work_stealing_iterative : public BnBSolverBase {

public:
  std::vector<std::chrono::duration<double>> timeFrames;
  std::chrono::_V2::system_clock::time_point lastUpdate;
  /**
   * @brief Constructor of a solver.
   *
   * @param irrelevance filter irrelevant jobs
   * @param fur use the fur Rule
   * @param gist take advantage of gists
   * @param addPreviously add the gist (with a flag) when starting to compute it
   * @param STtype specifies the STType
   */
  BnB_base_custom_work_stealing_iterative(bool irrelevance, bool fur, bool gist,
                                          bool addPreviously, int STtype,
                                          int maxAllowedParalellism = 6)
      : BnBSolverBase(irrelevance, fur, gist, addPreviously, STtype,
                      maxAllowedParalellism),
        workers(maxAllowedParalellism) {
    workers.resize(maxAllowedParalellism + 1);
    for (int i = 0; i < maxAllowedParalellism; i++) {
      auto tas = std::make_unique<TaskHolder>();
      workers[i] = std::move(tas);
    }
    workers[maxAllowedParalellism] = std::make_unique<SuspendedTaskHolder>();
  }
  int solve(int numMachine, const std::vector<int> &jobDuration) override {
    reset();
    resetLocals();
    // reset all private variables
    numMachines = numMachine;
    jobDurations = jobDuration;
    gistLength = numMachine + 1;
    // this includes the extra info on when the gist changes
    wrappedGistLength = numMachine + 2;
    // assume sorted
    assert(std::is_sorted(jobDurations.begin(), jobDurations.end(),
                          std::greater<int>()));
    offset = 1;

    initialLowerBound = trivialLowerBound();
    lowerBound = initialLowerBound;

    // initialize bounds (trivial nothing fancy here)
    initialUpperBound = lPTUpperBound();
    upperBound = initialUpperBound - 1;

    if (initialUpperBound == lowerBound) {
      hardness = Difficulty::trivial;
      return initialUpperBound;
    }

    // irrelevance
    lastRelevantJobIndex = jobDurations.size() - 1;
    if (irrelevance)
      lastRelevantJobIndex = computeIrrelevanceIndex(lowerBound);

    // tg_lower.run([this]
    //              { improveLowerBound(); });
    // RET
    offset = 1;
    fillRET();

    // ST for gists
    initializeST();
    // one JobSize left
    int i = lastRelevantJobIndex;
    // TODO last size can be done async
    while (i > 0 && jobDurations[--i] == jobDurations[lastRelevantJobIndex]) {
    }
    lastSizeJobIndex = i + 1;
    if (logInitialBounds)
      std::cout << "Initial Upper Bound: " << upperBound
                << " Initial lower Bound: " << lowerBound << std::endl;

    lastUpdate = std::chrono::high_resolution_clock::now();
    // start Computing
    std::vector<int> initialState(numMachines, 0);
    auto start = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> threads(0);
    threads.push_back(
        std::thread([&]() { // todo here initialize size of threadLocalvector
                            // nad make it to an array
          threadIndex = 0;
          initializeThreadLocalVector(numMachines);
          auto firstTask = std::make_shared<CustomTask>(initialState, 0);
          solvePartial(firstTask);
          while (!foundOptimal && !cancel) {
            auto toExecute = workers[threadIndex]->getNextTask();
            int count = 0;
            while (toExecute == nullptr && count++ < 5) {
              const auto stealFrom =
                  rand.nextInRange(maxAllowedParalellism - 1);
              toExecute = workers[stealFrom]->stealTasks();
            }
            while (toExecute == nullptr && count++ < 10) {
              const auto stealFrom = rand.nextInRange(maxAllowedParalellism);
              toExecute = workers[stealFrom]->stealTasks();
            }
            if (toExecute == nullptr)
              continue;
            logging(toExecute->state, toExecute->job, "continue work on");
            solvePartial(toExecute);
          }
        }));
    for (int i = 1; i < maxAllowedParalellism; i++) {
      threads.push_back(std::thread([&, i]() {
        // need to pass it especially as a value otherwise it
        // only takes the reference wich might be the next i
        // because thread creation is async i assume
        threadIndex = i;
        initializeThreadLocalVector(numMachines);
        // todo here initialize size of threadLocalvector
        while (!foundOptimal && !cancel) {
          auto toExecute = workers[threadIndex]->getNextTask();
          int count = 0;
          while (toExecute == nullptr && count++ < 5) {
            toExecute = workers[rand.nextInRange(maxAllowedParalellism - 1)]
                            ->stealTasks();
          }
          while (toExecute == nullptr && count++ < 10) {
            toExecute =
                workers[rand.nextInRange(maxAllowedParalellism)]->stealTasks();
          }
          if (toExecute == nullptr)
            continue;
          logging(toExecute->state, toExecute->job, "continue work on");
          solvePartial(toExecute);
        }
      }));
    }

    std::condition_variable mycond;
    std::mutex mtx;

    std::thread monitoringThread([&]() {
      // lastVisited Nodes was added as a workaround for getting stuck (pausing jobs but somehow not restarting them [should be fixed now])
      // int lastVisitedNodes = visitedNodes;
      while (!foundOptimal && !cancel) {
        // if (lastVisitedNodes == visitedNodes) {
        //     STInstance->resumeAllDelayedTasks();
        //     if (count++ > 3 ) addPreviously = false;
        // }

        // lastVisitedNodes = visitedNodes;

        double memoryUsage = getMemoryUsagePercentage();
        if (memoryUsage > 85) {
          // std::cout << "Memory usage high: " << memoryUsage
          //           << "%. Calling evictAll. "
          //           << ((std::chrono::duration<
          //                   double>)(std::chrono::high_resolution_clock::now() -
          //                            start))
          //                  .count()
          //           << std::endl;
          std::unique_lock lock(boundLock);
          STInstance->clear();

          // memoryUsage = getMemoryUsagePercentage();
          // std::cout << "Memory usage after evictAll: " << memoryUsage << "%"
          //           << std::endl;
        }
        // interuptable wait
        std::unique_lock<std::mutex> condLock(mtx);
        mycond.wait_for(condLock, std::chrono::milliseconds(5));
      }
      return;
    });
    threads[0].join();

    timeFrames.push_back(
        (std::chrono::high_resolution_clock::now() - lastUpdate));

    if (cancel) {
      monitoringThread.join();
      for (int i = 1; i < maxAllowedParalellism; i++) {
        threads[i].join();
      }
      // tg_lower.wait();
      return 0;
    }

    // foundOptimal = true;
    mycond.notify_all();

    // upper Bound is the next best bound so + 1 is the best found bound
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

    monitoringThread.join();
    // tg_lower.wait();
    for (int i = 1; i < maxAllowedParalellism; i++) {
      threads[i].join();
    }
    return upperBound + 1;
  }

  void getSubInstance(std::vector<int> &sub, int jobSize) {
    for (int i = 0; i < jobSize; i++) {
      sub[i] = jobDurations[i];
    }
    if (fillSubinstances) {
      int fillTo = lastRelevantJobIndex;
      int fillSize =
          jobDurations[lastRelevantJobIndex]; // TODO find a better way
      if (jobSize < lastRelevantJobIndex - 5) {
        fillTo -= 5;
        fillSize = jobDurations[fillTo];
      }
      for (int i = jobSize; i < fillTo; i++) {
        sub[i] = fillSize;
      }
      sub.resize(fillTo);
    } else {
      sub.resize(jobSize);
    }
  }
  void improveLowerBound() {
    int jobSize = (double)lastRelevantJobIndex *
                  0.7; // TODO so setzen, dass ein gewisser prozentteil des
                       // gesamtgewichtes drin ist
    std::vector<int> sub(lastRelevantJobIndex);
    while (!cancel && !foundOptimal) {
      lowerBounds_ret lowerSolver(foundOptimal, cancel);
      getSubInstance(sub, jobSize);
      int solvedSubinstance = lowerSolver.solve(sub, numMachines);
      if (solvedSubinstance > lowerBound)
        lowerBound = solvedSubinstance;
      if (lowerBound == upperBound + 1) {
        foundOptimal = true;
      }
      if (lowerBound > upperBound + 1) // TODO that should be an assert
      {
        throw std::runtime_error(
            "Subinstance found a lower bound " + std::to_string(lowerBound) +
            " that is higher than the current UpperBound " +
            std::to_string(upperBound));
      }
      jobSize += 4;
      if (jobSize >= lastRelevantJobIndex)
        break;
    }
  }

  void cleanUp() override {
    reset();
    resetLocals();
  }
  void cancelExecution() {
    cancel = true;
    if (STInstance != nullptr) {
      STInstance->cancelExecution();
    }
  }

private:
  // random
  FastRandom rand = FastRandom(12345);
  // Problem instance
  int numMachines;
  std::vector<int> jobDurations;

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
  ST_custom *STInstance = nullptr;

  // workStealing
  std::vector<std::shared_ptr<ITaskHolder>> workers;

  // Logging
  bool logNodes = false;
  bool logInitialBounds = false;
  bool logBound = false;
  bool detailedLogging = false;

  int lastRelevantJobIndex;

  // solve Subinstances
  bool useSubinstances = false;
  bool fillSubinstances = false;
  bool adaptiveSubinstances =
      true; // experimental for the future do sth. like depending on the variety
            // of jobsizes do fill subinstances or not or adapt the size of the
            // subinstance

  // for logging the amount of tasks that get executed even though the gist is
  // currently computed
  std::atomic<int> runWithPrev = 0;
  std::atomic<int> allPrev = 0;

  // only one jobsize left
  int lastSizeJobIndex;
  inline int trivialLowerBound() {
    return std::max(
        std::max(jobDurations[0],
                 jobDurations[numMachines - 1] + jobDurations[numMachines]),
        std::accumulate(jobDurations.begin(), jobDurations.end(), 0) /
            numMachines);
  }
  // TODO hier vermutlich besser mit referenz arbeiten
  void solvePartial(std::shared_ptr<CustomTask> task) {
    auto &[state, job, parentGroup, continueAt, r6, loopIndex] = *task;
    assert(std::is_sorted(state.begin(), state.end()));
    auto pointer =
        std::make_shared<CustomTaskGroup>(*workers[threadIndex], task);
    auto &tg = *pointer;
    switch (continueAt) {
    default: {
      if (foundOptimal || cancel) {
        //    early return should be enough (but
        //   what if another task is suspended well we can just return the
        //   suspended one s manually)
        //   redundancyFinish();
        finishGistless(parentGroup);
        return;
      }
      int makespan = state[numMachines - 1];

      visitedNodes.fetch_add(1, std::memory_order_relaxed);
      if (logNodes && visitedNodes % 1000000 == 0) {
        std::cout << "visited nodes: " << visitedNodes
                  << " current Bound: " << upperBound << " " << job
                  << " makespan " << makespan << std::endl;
        std::cout << "current state: ";
        for (int i : state)
          std::cout << " " << i;
        std::cout << std::endl;
      }

      // no further recursion necessary?
      if (job > lastRelevantJobIndex) {
        updateBound(makespan);
        finishGistless(parentGroup);
        return;
      } else if (makespan > upperBound) {
        finishGistless(parentGroup);
        return;
      }
      // 3 jobs remaining
      else if (job == lastRelevantJobIndex - 2) {
        std::vector<int> one = state;
        lpt(one, job);
        std::vector<int> two = state;
        two[1] += jobDurations[job++];
        lpt(two, job);
        finishGistless(parentGroup);
        return;
      }
      // Rule 5
      else if (job >= lastSizeJobIndex) { // Rule 5
        // TODO it should be able to do this faster (at least for many jobs with
        // the same size) by keeping the state in a priority queu or sth like
        // that
        // TODO there is a condition which says, wether that instance can be
        // solved that might be faster than running lpt
        std::vector<int> one = state;
        lpt(one, job);
        finishGistless(parentGroup);
        return;
      }
      logging(state, job, "before lookup");

      if (gist) {
        int exists = STInstance->exists(state, job);

        logging(state, job, exists);
        switch (exists) {
        case 2:
          logging(state, job, "gist found");
          finishGistless(parentGroup);
          return;
        case 1:
          if (addPreviously &&
              job <= lastSizeJobIndex *
                         0.8) { // TODO check but that should be usefull (maybe
                                // not for 20% but a fixed number)
            // int repeated = 0;
            // while (STInstance->exists(state, job) == 1 && repeated++ < 1) {
            logging(state, job, "suspend");
            //   if (makespan > upperBound || foundOptimal || cancel) {
            //     logging(state, job, "after not worth continuing");
            //     finishGistless(parentGroup);
            //     return;
            //   }
            STInstance->addDelayed(task);
            earlyReturn();
            return;
            // logging(state, job, "restarted");
            // if (makespan > upperBound || foundOptimal || cancel) {
            //   logging(state, job, "after not worth continuing");
            //   finishGistless(parentGroup);
            //   return;
            // }
            // }

            // if (STInstance->exists(state, job) == 2) {
            //   logging(state, job, "after restard found");
            //   finishGistless(parentGroup);
            //   return;
            // }
          }
          break;
        default:
          logging(state, job, "gist not found");
          if (addPreviously)
            STInstance->addPreviously(state, job);
        }
      }
    }
    case 4: {
      // FUR
      if (fur) {
        int start = (numMachines - 1);
        if (loopIndex >= 0) {
          if (state[loopIndex] + jobDurations[job] <= upperBound &&
              lookupRetFur(state[loopIndex], jobDurations[job], job)) {
            logging(state, job, "after add");
            finished(parentGroup, state, job);
            return;
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
            threadLocalStateVector = state;
            assert(threadLocalStateVector == state);
            // might not need to copy here but carefully
            threadLocalStateVector[i] += jobDurations[job];
            resortAfterIncrement(local, i);
            loopIndex = i;
            logging(local, job + 1, "FurChild");
            tg.run(local, job + 1); // TODO here is a problem
            if (tg.wait(4)) {
              logging(state, job, "wait for fur child");
              earlyReturn();
              return;
            }
          }
        }
      }
      logging(state, job, "after Fur");

      int endState = numMachines;
      // index of assignments that are currently ignored by Rule 6 (same Ret
      // entry than the previous job and can therefore be ignored)

      // index of assignments, that would lead to a gist that is already handled
      // somewhere else
      // Rule 4
      if (numMachines > lastRelevantJobIndex - job + 1)
        endState = lastRelevantJobIndex - job + 1;

      assert(endState <= numMachines);
      // base case
      executeBaseCase(endState, state, job, r6, tg);

      logging(state, job, "wait for tasks to finish");
      if (tg.wait(1)) {
        earlyReturn();
        return;
      }
    }
    case 1:
      // executeDelayed(delayed, state, job, tg); just add them either way and
      // let the suspend hanlde it
      logging(state, job, "executeR6");
      executeR6Delayed(r6, state, job, tg);
      if (tg.wait(10)) {
        logging(state, job, "wait for R6 to finish");
        earlyReturn();
        return;
      }
    case 10:
      break;
    }
    if (job == 0) {
      foundOptimal = true; // to ensure ending
    }
    logging(state, job, "after add");
    finished(parentGroup, state, job);
    return;
  }
  inline void executeR6Delayed(const std::vector<int> &repeat,
                               const std::vector<int> &state, const int job,
                               CustomTaskGroup &tg) {
    for (int i : repeat) {
      if (!lookupRet(state[i], state[i - 1], job)) { // Rule 6
        threadLocalStateVector = state;
        threadLocalStateVector[i] += jobDurations[job];
        resortAfterIncrement(threadLocalStateVector, i);
        logging(threadLocalStateVector, job + 1, "Rule 6 from recursion");
        tg.run(threadLocalStateVector, job + 1);
      }
    }
  }

  inline void executeBaseCase(const int endState, const std::vector<int> &state,
                              const int job, std::vector<int> &repeat,
                              CustomTaskGroup &tg) {
    for (int i = 0; i < endState; i++) {
      if ((i > 0 && state[i] == state[i - 1]) ||
          state[i] + jobDurations[job] > upperBound)
        continue; // Rule 1 + check directly wether the new state would be
                  // feasible
      if (i > 0 && lookupRet(state[i], state[i - 1], job)) { // Rule 6
        repeat.push_back(i);
        continue;
      }
      threadLocalStateVector = state;
      assert(threadLocalStateVector.size() == static_cast<size_t>(numMachines));
      threadLocalStateVector[i] += jobDurations[job];
      assert(threadLocalStateVector.size() == static_cast<size_t>(numMachines));

      resortAfterIncrement(threadLocalStateVector, i);

      // filter delayed / solved assignments directly before spawning the tasks
      if (gist) {
        auto ex = STInstance->exists(threadLocalStateVector, job + 1);
        switch (ex) {
        case 0:
          logging(threadLocalStateVector, job + 1, "child from recursion");
          tg.run(threadLocalStateVector, job + 1);
          break;
        case 1:
          // delete next;
          // delayed.push_back(i); //  TODO handle suspended tasks
          logging(threadLocalStateVector, job + 1,
                  "child from recursion (most liekly delyed)");
          tg.run(threadLocalStateVector,
                 job + 1); // todo theoretically it can instantly added as a
                           // suspended task
          break;
        default:
          continue;
          break;
        }
      } else {
        tg.run(threadLocalStateVector, job + 1);
      }
    }
  }

  template <typename T>
  void logging(const std::vector<int> &state, int job, T message = "") {
    if (!detailedLogging)
      return;

    std::stringstream gis;
    gis << message << " ";
    for (auto vla : state)
      gis << vla << ", ";
    // gis << " => ";
    // for (auto vla : STInstance->computeGist(state, job))
    //   gis << vla << ", ";
    gis << " Job: " << job;
    std::cout << gis.str() << std::endl;
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
   * @brief tightens the upper bound and updates the offset for the RET
   */
  void updateBound(int newBound) {
    assert(newBound >= lowerBound);
    if (newBound == lowerBound) {
      foundOptimal = true;
      // STInstance->resumeAllDelayedTasks(); // TODO work with a finished flag 
      // not necessary with custom tasks
                                           // that should be more performant
    }

    if (logBound)
      std::cout << "try Bound " << newBound << lowerBound << std::endl;

    if (newBound > upperBound)
      return;
    if (logBound)
      std::cout << "new Bound " << newBound << std::endl;
    std::unique_lock lock(boundLock,
                          std::try_to_lock); // this one does not appear often
                                             // so no need to optimize that
    while (!lock.owns_lock()) {
      std::this_thread::yield();
      if (newBound > upperBound.load())
        return; // TODO check wether this works fine because we end an
                // exploration path even though the bound is not yet updated
      lock.try_lock();
    }
    if (newBound > upperBound)
      return;
    const int newOffset = initialUpperBound - (newBound - 1);
    if (gist) // it is importatnt to d othe bound update on the ST before
              // updating the offset/upperBound, because otherwise the exist
              // might return a false positive (i am not 100% sure why)
    {
      STInstance->prepareBoundUpdate();
    }
    upperBound.store(newBound - 1);
    offset.store(newOffset);
    if (gist) // it is importatnt to d othe bound update on the ST before
              // updating the offset/upperBound, because otherwise the exist
              // might return a false positive (i am not 100% sure why)
    {
      STInstance->boundUpdate(newOffset);
    }
    upperBound.store(newBound - 1);
    offset.store(newOffset);

    auto newTime = std::chrono::high_resolution_clock::now();
    timeFrames.push_back((std::chrono::duration<double>)(newTime - lastUpdate));
    lastUpdate = newTime;
    if (logBound)
      std::cout << "new Bound " << newBound << "finished" << std::endl;
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
   * @brief find index, to when jobs are no longer relevant regarding the given
   * Bound.
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
  double getMemoryUsagePercentage() {
    long totalPages = sysconf(_SC_PHYS_PAGES);
    long availablePages = sysconf(_SC_AVPHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);

    if (totalPages > 0) {
      long usedPages = totalPages - availablePages;
      double usedMemory = static_cast<double>(usedPages * pageSize);
      double totalMemory = static_cast<double>(totalPages * pageSize);
      return (usedMemory / totalMemory) * 100.0;
    }
    return -1.0; // Fehlerfall
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
    for (auto u = 0; u <= initialUpperBound;
         u++) { // TODO there is some point u where everithing before is a 1 and
                // after it is a 2 so 2 for loops could be less branches
      if (u + jobDurations[lastRelevantJobIndex] > initialUpperBound) {
        RET[lastRelevantJobIndex][u] = 1;
      } else {
        RET[lastRelevantJobIndex][u] = 2;
      }
    }
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

  void resetLocals() {
    numMachines = 0;
    jobDurations.clear();
    upperBound = 0;
    lowerBound = 0;
    initialUpperBound = 0;
    offset = 0;
    lastRelevantJobIndex = 0;
    lastSizeJobIndex = 0;
    cancel = false;
    foundOptimal = false;
    visitedNodes = 0;
    RET.clear();
    timeFrames.clear();
    if (STInstance != nullptr) {
      STInstance->clear();
      delete STInstance;
    }
    STInstance = nullptr;
  }

  // maybe do a resort while increment? because we need to rewrite the vector anyway therefore might do it together
  // basically insertion sort
  inline void resortAfterIncrement(std::vector<int> &vec, size_t index) {
    assert(index < vec.size());
    int incrementedValue = vec[index];
    // since it was an increment the newPosIt can only be at the same index or
    // after
    auto newPosIt =
        std::upper_bound(vec.begin() + index, vec.end(), incrementedValue);
        assert(static_cast<std::size_t>(gistLength) == vec.size() + 1);
    size_t newPosIndex = std::distance(vec.begin(), newPosIt);
    if (newPosIndex == index) {
      return;
    }
    std::rotate(vec.begin() + index, vec.begin() + index + 1,
                vec.begin() + newPosIndex);
    assert(std::is_sorted(vec.begin(), vec.end()));
  }
  inline void finished(std::shared_ptr<CustomTaskGroup> parentGroup,
                       std::vector<int> &state, int job) {
    if (parentGroup != nullptr)
      parentGroup->unregisterChild();
    if (gist)
      STInstance->addGist(state, job);
  }

  inline void finishGistless(std::shared_ptr<CustomTaskGroup> parentGroup) {
    if (parentGroup != nullptr)
      parentGroup->unregisterChild();
  }
  inline void earlyReturn() {}

  void initializeST() {
    switch (STtype) {
      // STImpl currently not maintained
    // case 0:
    //     STInstance = new STImpl(lastRelevantJobIndex + 1, offset, RET,
    //     numMachines); break;
    case 2:
      STInstance = new ST_combined(lastRelevantJobIndex + 1, offset, RET,
                                   numMachines, *workers[maxAllowedParalellism],
                                   2, maxAllowedParalellism);
      break;
    case 5:
      STInstance = new ST_combined(lastRelevantJobIndex + 1, offset, RET,
                                   numMachines, *workers[maxAllowedParalellism],
                                   0, maxAllowedParalellism);
      break;
    default:
      STInstance = new STImplSimplCustomLock(
          lastRelevantJobIndex + 1, offset, RET, numMachines,
          *workers[maxAllowedParalellism], 0, maxAllowedParalellism);
    }
  }
  friend class CustomTaskGroup;
};

#endif