// IConcurrentHashMap.h
#ifndef IConcurrentHashMapCombined_H
#define IConcurrentHashMapCombined_H
#include "../../../structures/DelayedTasksList.hpp"
#include "../../../structures/GistStorage.hpp"
#include "../../../threadLocal/threadLocal.h"
#include "hashing/hashing.hpp"
#include <cassert>
#include <memory>
#include <string>
#include <tbb/tbb.h>
#include <vector>

class IConcurrentHashMapCombined {
public:
  IConcurrentHashMapCombined(
      std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage)
      : Gist_storage(Gist_storage){};

  virtual ~IConcurrentHashMapCombined() = default;

  virtual DelayedTasksList *insert(int *gist, bool value) = 0;
  virtual void reinsertGist(int *gist, DelayedTasksList *delayed) = 0;
  virtual int find(int *key) = 0;
  virtual void clear() = 0;
  virtual bool tryAddDelayed(std::shared_ptr<CustomTask> &task, int *gist) = 0;
  int *createGistEntry(int *gist) {
    return Gist_storage[threadIndex]->push(gist);
  }
  void deleteGistEntry() { Gist_storage[threadIndex]->pop(); }
  virtual std::vector<DelayedTasksList *> getDelayed() = 0;
  virtual std::pair<std::vector<std::pair<int *, DelayedTasksList *>>,
                    std::vector<DelayedTasksList *>>
  getNonEmptyGists(const int newOffset) = 0;

private:
  std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage;
};

#endif