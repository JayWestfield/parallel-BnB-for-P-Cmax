#include <vector>
#include <tbb/task.h>
#include <tbb/task_group.h>
#include "../../hashing/hashFuntions.hpp"

class IDelayedmap {
   
public:
    virtual ~IDelayedmap() = default;

    // Methode zum Einfügen eines Schlüssels und eines Suspend Points
    virtual void insert(const std::vector<int>& key, tbb::task::suspend_point suspendPoint) = 0;

    // Methode zum Resumieren der Suspend Points, die einem Schlüssel zugeordnet sind
    virtual void resume(const std::vector<int>& key) = 0;

    // Methode zum Resumieren aller Suspend Points
    virtual void resumeAll() = 0;
     struct VectorHasher
    {
        inline void hash_combine(std::size_t &s, const int &v) const
        {
            s ^= hashing::hash_int(v) + 0x9e3779b9 + (s << 6) + (s >> 2);
        }
        size_t hash(const std::vector<int> &vec)
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        size_t operator()(const std::vector<int> &vec) const
        {
            size_t h = 17;
            for (auto entry : vec)
            {
                hash_combine(h, entry);
            }
            return h;
            // since this is called very often and the vector size depends on the instance it should be possible to optimize that in a way vec.size() does not need to be called
            // return murmur_hash64(vec);
        }
        bool equal(const std::vector<int> &a, const std::vector<int> &b) const
        {
            return std::equal(a.begin(), a.end(), b.begin());
        }
    };
};
