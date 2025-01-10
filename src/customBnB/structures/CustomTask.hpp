// Context:
// waits nach base, delayed, R6
// delayed brauchen wir nicht mehr dank des suspends
// wait nach R6 bracuht keinen zusaätzlichen context
// den gist check vlt trotzdem machen um taskerzeugung zu minimieren ( ist auch
// wieder eine speicherallokation) int der sagt wo wait aufgerufen wurde, repeat
// vector of ints ()
// TODO future might replace vectors with custom fixed size vectors precompiled
// for different lengths with templates
#ifndef CustomTask_impl
#define CustomTask_impl

#include <memory>
#include <vector>
class CustomTaskGroup;
struct CustomTask {
  CustomTask(std::vector<int> state, int job,
             std::shared_ptr<CustomTaskGroup> parentGroup = nullptr,
             int continueAt = 0, std::vector<int> r6 = {}, int loopIndex = -1)
      : state(state), job(job), parentGroup(parentGroup),
        continueAt(continueAt), r6(r6), loopIndex(loopIndex) {}
  std::vector<int> state;
  int job;
  std::shared_ptr<CustomTaskGroup> parentGroup;
  int continueAt;
  std::vector<int> r6;
  int loopIndex;
};

#endif