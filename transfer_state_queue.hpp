#ifndef TRANSFER_STATE_QUEUE_HPP_
#define TRANSFER_STATE_QUEUE_HPP_

#include "wait_ack_queue.hpp"

struct TransferState {
  int socket;
  int sendPos;
  long lastSendTime;
  long lastAckTime;
  WaitAckQueue waitAckQueue;

  TransferState(int s, long t) : socket(s), sendPos(0), lastSendTime(t), lastAckTime(t) {
  }
};

struct TransferStateCompartor {
  bool operator() (TransferState*& lhs, TransferState*& rhs) {
    if (lhs->lastSendTime < rhs->lastSendTime) {
      return true;
    } else if (lhs->lastSendTime == rhs->lastSendTime) {
      return lhs->lastAckTime < rhs->lastAckTime;
    } else {
      return false;
    }
  }
};

struct TransferStateFinalizer {
  bool operator() (TransferState* state) {
    delete state;
  }
};

typedef HashedPriorityQueue<int, TransferState*, TransferStateCompartor, TransferStateFinalizer> TransferStateQueue;

#endif  // TRANSFER_STATE_QUEUE_HPP_
