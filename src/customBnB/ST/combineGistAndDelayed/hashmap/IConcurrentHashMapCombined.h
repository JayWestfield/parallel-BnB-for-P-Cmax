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
  // insert with bool is a relic and should be redone with 2 methods addGist and addPrev or sth liuke that
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
  virtual void
  getNonEmptyGists(tbb::concurrent_queue<std::pair<int *, DelayedTasksList *>>
                       &maybeReinsert,
                   tbb::concurrent_queue<DelayedTasksList *> &restart,
                   const int newOffset) = 0;

private:
  std::vector<std::unique_ptr<GistStorage<>>> &Gist_storage;
};

#endif