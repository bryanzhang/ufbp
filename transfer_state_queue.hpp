#ifndef TRANSFER_STATE_QUEUE_HPP_
#define TRANSFER_STATE_QUEUE_HPP_

#include "wait_ack_queue.hpp"

struct TransferState {
  int socket;
  int sendPos;
  int chunks;
  long lastSendTime;
  long lastAckTime;
  WaitAckQueue waitAckQueue;

  TransferState(int s, int c, long t) : socket(s), sendPos(0), chunks(c), lastSendTime(t), lastAckTime(t) {
  }
};

struct TransferStateCompartor {
  bool operator() (TransferState*& lhs, TransferState*& rhs) {
    long lhsTime, rhsTime;
    if (lhs->sendPos < lhs->chunks) {
      lhsTime = lhs->lastSendTime;
    } else {
      lhsTime = lhs->lastAckTime;
    }
    if (rhs->sendPos < rhs->chunks) {
      rhsTime = rhs->lastSendTime;
    } else {
      rhsTime = rhs->lastAckTime;
    }

    return lhsTime < rhsTime;
  }
};

struct TransferStateFinalizer {
  bool operator() (TransferState* state) {
    delete state;
  }
};

typedef HashedPriorityQueue<int, TransferState*, TransferStateCompartor, TransferStateFinalizer> TransferStateQueue;

#endif  // TRANSFER_STATE_QUEUE_HPP_
