#ifndef URI_STATE_QUEUE_HPP_
#define URI_STATE_QUEUE_HPP_

#include <cstdio>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "transfer_state_queue.hpp"
#include "bit_vector.hpp"
#include "ufbp_common.hpp"

struct UriState {
  char* uri;
  long fileLength;
  long chunks;
  long lastModifiedDate;
  long lastSendTime;
  long lastAckTime;
  void* mmap_addr;
  int fd;
  TransferStateQueue transferStateQueue;
  BitVector sendMap;

  UriState(char* u, long fl, long md, long t, int socket) : uri(u), fileLength(fl), chunks(fl / CHUNK_SIZE), lastModifiedDate(md), lastSendTime(t), lastAckTime(t), sendMap() {
    fd = open(u, O_RDWR, 0600);
    if (fd == -1) {
      perror("open file error");
      exit(1);
    }
    mmap_addr = mmap(0, fileLength, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
    if (mmap_addr == MAP_FAILED) {
      perror("mmap error");
      exit(1);
    }

    TransferState* state = new TransferState(socket, (fl + CHUNK_SIZE - 1) / CHUNK_SIZE, t);
    transferStateQueue.PushEntry(socket, state);
  }

  void add(TransferState* state) {
    transferStateQueue.PushEntry(state->socket, state);
    sendMap.clear();
  }

  ~UriState() {
    close(fd);
    munmap(mmap_addr, fileLength);
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
