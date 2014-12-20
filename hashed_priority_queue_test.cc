#include <cassert>
#include "hashed_priority_queue.hpp"

struct ValueType {
  int id;
  int value;
  ValueType(int i, int v) : id(i), value(v) {}
};

class Comparator {
 public:
  bool operator () (ValueType*& lhs, ValueType*& rhs) {
    return lhs->value < rhs->value;
  }
};

int main(int argc, char** argv) {
  HashedPriorityQueue<int, ValueType*, Comparator> que;
  que.PushEntry(3, new ValueType(3, -3));
  que.PushEntry(7, new ValueType(7, -7));
  que.PushEntry(1, new ValueType(1, -1));
  que.PushEntry(2, new ValueType(2, -2));
  que.PushEntry(6, new ValueType(6, -6));
  que.PushEntry(8, new ValueType(8, -8));
  que.PushEntry(11, new ValueType(11, -5));
  que.PushEntry(13, new ValueType(13, -9));

  ValueType* v;
  que.GetConstEntry(13, &v);
  assert(v->id == 13);
  assert(v->value == -9);
  assert(que.Count() == 8);
  
  return 0;
}
