#include <condition_variable>
#include <functional>
#include <thread>

class memoryMonitor {
public:
  // Modify constructor to accept a function
  memoryMonitor(std::atomic<bool> &continueExecution,
                std::function<void()> evictionFunc)
      : continueExecution(continueExecution), evictionFunc(evictionFunc) {

    monitoringThread = std::thread([this]() {
      while (this->continueExecution) {
        double memoryUsage = getMemoryUsagePercentage();
        if (memoryUsage > 70) {
          // Call the provided eviction function
          this->evictionFunc();
        }
        std::unique_lock<std::mutex> condLock(this->mtx);
        mycond.wait_for(condLock, std::chrono::milliseconds(5));
      }
    });
  }

  ~memoryMonitor() {
    if (monitoringThread.joinable())
      monitoringThread.join();
  }

  void notify() { mycond.notify_all(); }

private:
  std::function<void()> evictionFunc; // Function to be called for eviction
  std::condition_variable mycond;
  std::mutex mtx;
  std::atomic<bool> &continueExecution;
  std::thread monitoringThread;

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
};
