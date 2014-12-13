#ifndef SERVER_STATES_HPP_
#define SERVER_STATES_HPP_

#include <queue>
#include "ufbp_common.hpp"

struct BufferUnit {
  unsigned char* pos;
  unsigned short len;
};

struct PullRequest {
  unsigned long reqId;
  char* uri;
  int len;
};

struct ServerStates {
  int socket;
  int tcpSocket;
  unsigned char buffer[BUFFER_SIZE];
  std::queue<BufferUnit> bufQueue;
  std::queue<PullRequest> reqQueue;

  inline ServerStates() : socket(-1), tcpSocket(-1), bufQueue(), reqQueue() {
  }
};

extern ServerStates g_svStates;

#endif  // SERVER_STATES_HPP_
