#ifndef HASHED_PRIORITY_QUEUE_HPP_
#define HASHED_PRIORITY_QUEUE_HPP_

#include <cassert>
#include <vector>
#include <ext/hash_map>

template <class ValType>
class EmptyValFinalizer {
 public:
  bool operator () (const ValType& type) { }
};

template <class KeyType, class ValType, class CMP_FUNCTOR=std::less<ValType>, class FINALIZER = EmptyValFinalizer<ValType> >
class HashedPriorityQueue {
 protected:
  struct EntryType {
    KeyType k;
    ValType v;
    int heap_pos;
    EntryType():heap_pos(-1) {}
    EntryType(KeyType _k, ValType _v, int _heap_pos):
      k(_k),v(_v),heap_pos(_heap_pos) {}
  };
  typedef std::vector<EntryType*> HeapType;
  typedef __gnu_cxx::hash_map<KeyType, EntryType*> MapType;
  typedef typename __gnu_cxx::hash_map<KeyType, EntryType*>::const_iterator MapItrType;
 public:
  HashedPriorityQueue() {}
  ~HashedPriorityQueue() {
    // not virtual, so only appropriate to use it in terms of "combination"
    Clear();
  }
  // get non-modifiable elem reference
  bool GetConstEntry(KeyType k, ValType* v) const {
    MapItrType itr = map_.find(k);
    if (itr == map_.end()) {
      return false;
    }
    *v = (itr->second)->v;
    return true;
  }
  // get element count
  const size_t Count() const {
    return heap_.size();
  }
  bool IsEmpty() const { return Count() == 0; }
  // pop specified-key entry
  bool PopEntry(KeyType k, ValType* v) {
    // find in hash, get pos in the heap
    MapItrType itr = map_.find(k);
    if (itr == map_.end()) {
      return false;
    }
    *v = (itr->second)->v;
    PopEntry(itr);
    return true;
  }
  // pop the least recent use entry
  void PopLRUEntry(KeyType* k, ValType* v) {
    assert(!IsEmpty());
    *k = (heap_[0])->k;
    *v = (heap_[0])->v;
    MapItrType itr = map_.find(*k);
    delete itr->second;
    map_.erase(*k);
 
    heap_[0]->heap_pos = Count() - 1;
    heap_[Count() - 1]->heap_pos = 0;
    std::swap(heap_[0], heap_[Count() - 1]);
    heap_.pop_back();
    adjust_heap(0);
  }
  // push entry into cache
  void PushEntry(KeyType k, ValType v) {
    // if the elem with the key exist, pop it, change value, push it again
    MapItrType itr = map_.find(k);
    EntryType* entry = new EntryType(k, v, Count());
    if (itr != map_.end()) {
      PopEntry(itr);
    }
    map_[k] = entry;
    heap_.push_back(entry);
    adjust_heap_from_bottom();
  }
  void Clear() {
    FINALIZER finalizer;
    // delete data first
    for (int i = 0; i < heap_.size(); ++i) {
      finalizer(heap_[i]->v);
      delete heap_[i];
    }
    heap_.clear();
    map_.clear();
  }
 protected:
  HeapType heap_;
  MapType map_;
  void PopEntry(MapItrType itr) {
    // erase in hash map
    int pos = (itr->second)->heap_pos;
    map_.erase(itr->first);
    // delete original data
    delete itr->second;
    // swap the one with the last one in the heap, then erase in the heap
    // then adjust in the sub heap
    heap_[pos]->heap_pos = Count() - 1;
    heap_[Count() - 1]->heap_pos = pos;
    std::swap(heap_[pos], heap_[Count() - 1]);
    heap_.pop_back();
    adjust_heap(pos);
  }
  void adjust_heap(int pos) {
    // only need to change heap pos additionally
    int count = Count();
    CMP_FUNCTOR func;
    EntryType* pivot = heap_[pos];
    for (int tocmp = pos * 2 + 1; tocmp < count; pos = tocmp, tocmp *= 2, ++tocmp) {
      if (tocmp + 1 < count && func(heap_[tocmp + 1]->v,
heap_[tocmp]->v)) {
        ++tocmp;
      }
      if (func(pivot->v, heap_[tocmp]->v)) {
        break;
      }
      heap_[pos] = heap_[tocmp];
      heap_[pos]->heap_pos = pos;
    }
    heap_[pos] = pivot;
    pivot->heap_pos = pos;
  }
  void adjust_heap_from_bottom() {
    // only need to change heap pos additionally
    CMP_FUNCTOR func;
    int pos = Count() - 1;
    EntryType* pivot = heap_[pos];
    for (int parent = (pos - 1) / 2;
        pos > 0 && func(pivot->v, heap_[parent]->v);
        pos = parent, --parent, parent /= 2) {
      heap_[pos] = heap_[parent];
      heap_[pos]->heap_pos = pos;
    }
    heap_[pos] = pivot;
    pivot->heap_pos = pos;
  }
};

#endif  // HASHED_PRIORITY_QUEUE_HPP_
