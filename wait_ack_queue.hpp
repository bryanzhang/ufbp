#ifndef WAIT_ACK_QUEUE_HPP_
#define WAIT_ACK_QUEUE_HPP_

#include "hashed_priority_queue.hpp"

struct ChunkState {
  long lastSendTime;
  int pos;
};

struct ChunkStateComparator {
  bool operator() (ChunkState& lhs, ChunkState& rhs) {
     if (lhs.lastSendTime < rhs.lastSendTime) {
       return true;
     } else if (lhs.lastSendTime == rhs.lastSendTime) {
       return lhs.pos < rhs.pos;
     } else {
       return false;
     }
  }
};

typedef HashedPriorityQueue<int, ChunkState, ChunkStateComparator> WaitAckQueue;

#endif  // WAIT_ACK_QUEUE_HPP_
