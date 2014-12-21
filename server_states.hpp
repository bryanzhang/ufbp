#ifndef SERVER_STATES_HPP_
#define SERVER_STATES_HPP_

#include <queue>
#include <ext/hash_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "ufbp_common.hpp"
#include "tcp_socket_state.hpp"
#include "scheduler.hpp"
#include "fileinfo.hpp"

struct PullRequest {
  unsigned long reqId;
  char* uri;
  int len;
};

struct ServerStates {
  unsigned char transferBuffer[CHUNK_SIZE];
  int transferBufferPos;
  int transferBufferOutPos;

  struct epoll_event events[20];
  int epfd;

  int socket;
  int tcpSocket;

  struct sockaddr_in si_client;

  __gnu_cxx::hash_map<int, TcpSocketState*> socketsPool;
  Scheduler scheduler;
  std::queue<unsigned short> transferBufferQueue;

  inline ServerStates() : transferBufferPos(0), transferBufferOutPos(0), epfd(-1), socket(-1), tcpSocket(-1) {
    si_client.sin_family = AF_INET;
    si_client.sin_port = htons(UFBP_CLIENT_PORT);
    si_client.sin_addr.s_addr = htonl(-1);
  }
};

extern ServerStates g_svStates;

#endif  // SERVER_STATES_HPP_
