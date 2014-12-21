#ifndef URI_STATE_QUEUE_HPP_
#define URI_STATE_QUEUE_HPP_

#include "transfer_state_queue.hpp"
#include "bit_vector.hpp"

struct UriState {
  char* uri;
  long fileLength;
  long lastModifiedDate;
  long lastSendTime;
  long lastAckTime;
  TransferStateQueue transferStateQueue;
  BitVector sendMap;

  UriState(char* u, unsigned long fl, unsigned long md, unsigned long t, int socket) : uri(u), fileLength(fl), lastModifiedDate(md), lastSendTime(t), lastAckTime(t), sendMap() {
    TransferState* state = new TransferState(socket, t);
    transferStateQueue.PushEntry(socket, state);
  }
};

struct UriStateComparator {
  bool operator() (UriState*& lhs, UriState*& rhs) {
    if (lhs->lastSendTime < rhs->lastSendTime) {
      return true;
    } else if (lhs->lastSendTime == rhs->lastSendTime) {
      return lhs->lastAckTime < rhs->lastAckTime;
    } else {
      return false;
    }
  }
};

struct UriStateFinalizer {
  bool operator() (UriState* state) {
    delete state;
  }
};

typedef HashedPriorityQueue<char*, UriState*, UriStateComparator, UriStateFinalizer> UriStateQueue;

#endif  // URI_STATE_QUEUE_HPP_
