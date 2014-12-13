#ifndef SERVER_STATES_HPP_
#define SERVER_STATES_HPP_

#include <queue>
#include <ext/hash_map>
#include <sys/epoll.h>
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

struct TcpSocketState {
  unsigned char buffer[128 * 1024];
  unsigned char outBuffer[1024];  // TODO(junhaozhang): should use public.
  int socket;
  int state;
  int bufferPos;
  int readPos;
};

struct ServerStates {
  struct epoll_event events[20];
  int epfd;

  int socket;
  int tcpSocket;

  __gnu_cxx::hash_map<int, TcpSocketState*> socketsPool;

  std::queue<BufferUnit> bufQueue;
  std::queue<PullRequest> reqQueue;

  inline ServerStates() : epfd(-1), socket(-1), tcpSocket(-1), bufQueue(), reqQueue() {
  }
};

extern ServerStates g_svStates;

#endif  // SERVER_STATES_HPP_
