#ifndef SERVER_STATES_HPP_
#define SERVER_STATES_HPP_

#include <queue>
#include <ext/hash_map>
#include <sys/epoll.h>
#include "ufbp_common.hpp"
#include "tcp_socket_state.hpp"
#include "scheduler.hpp"
#include "fileinfo.hpp"

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
  struct epoll_event events[20];
  int epfd;

  int socket;
  int tcpSocket;

  __gnu_cxx::hash_map<int, TcpSocketState*> socketsPool;
  Scheduler scheduler;

  inline ServerStates() : epfd(-1), socket(-1), tcpSocket(-1) {
  }
};

extern ServerStates g_svStates;

#endif  // SERVER_STATES_HPP_
