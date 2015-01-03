#ifndef SERVER_STATES_HPP_
#define SERVER_STATES_HPP_

#include <list>
#include <ext/hash_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <queue>
#include "ufbp_common.hpp"
#include "uri_state.hpp"
#include "transfer_state.hpp"
#include "socket_util.hpp"
#include "string_hash.hpp"

struct ServerStates {
  // milliseconds since epoch.
  long now;

  // resId related.
  unsigned short hostId;
  unsigned int secs;
  unsigned short inc;

  // epoll related
  int epfd;
  struct epoll_event events[20];

  // tcp server socket.
  int tcpSocket;

  // udp broadcast socket.
  int broadcastSocket;

  // broadcast address.
  struct sockaddr_in siBroadcast;

  // pending connections
  std::list<TransferState*> pendingStateList;  // 按最后读取时间升序排
  __gnu_cxx::hash_map<int, std::list<TransferState*>::iterator> pendingStateMap;

  __gnu_cxx::hash_map<std::string, UriState*> uriStates;

  // transfer state queue.
  std::list<TransferState*> transferStateList;  // 按照最后ack时间升序排列
  __gnu_cxx::hash_map<int, std::list<TransferState*>::iterator> transferStateMap;

  unsigned char tcpOutBuffer[BUFFER_SIZE];  // global tcp outbuffer, no state.

  // udp transfer buffer related states.
  unsigned char transferBuffer[TRANSFER_BUFFER_SIZE];
  int transferBufferReadPos;
  int transferBufferWritePos;
  std::queue<unsigned short> transferQueue;

  inline ServerStates() : now(0), hostId(88), secs(0), inc(0),
      epfd(-1), tcpSocket(-1), broadcastSocket(-1), 
      transferBufferReadPos(0), transferBufferWritePos(0) {
    siBroadcast.sin_family = AF_INET;
    siBroadcast.sin_port = htons(UFBP_CLIENT_PORT);
    siBroadcast.sin_addr.s_addr = htonl(-1);
  }

  bool init() {
    return initUdp() && initTcp() && initEpoll();
  }

  // create udp socket and bind.
  bool initUdp() {
    if ((broadcastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
      perror("Broadcast UDP created socket error");
      return false;
    }
    int so_broadcast = 1;
    if (setsockopt(broadcastSocket, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)), 0) {
      perror("Broadcast UDP set socket error");
      close(broadcastSocket);
      broadcastSocket = -1;
      return false;
    }

    struct sockaddr_in si_server;
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(UFBP_SERVER_PORT);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(broadcastSocket, (struct sockaddr*)&si_server, sizeof(si_server)) == -1) {
      perror("Bind error");
      close(broadcastSocket);
      broadcastSocket = -1;
      return false;
    }
    setnonblocking(broadcastSocket);
    return true;
  }

  // create tcp socket and bind.
  bool initTcp() {
    if ((tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
      perror("TCP server socket created socket error");
      return false;
    }
    int opt = SO_REUSEADDR;
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
      perror("TCP server set socket error");
      close(tcpSocket);
      tcpSocket = -1;
      return false;
    }

    struct sockaddr_in si_server;
    si_server.sin_family = AF_INET;
    si_server.sin_port = htons(UFBP_SERVER_TCP_PORT);
    si_server.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(tcpSocket, (struct sockaddr*)&si_server, sizeof(si_server)) == -1) {
      perror("Bind tcp port error");
      close(tcpSocket);
      tcpSocket = -1;
      return false;
    }
    if (listen(tcpSocket, 128) == -1) {
      perror("Listen error");
      close(tcpSocket);
      tcpSocket = -1;
      return false;
    }
    return true;
  }

  // create epoll and register
  bool initEpoll() {
    epfd = epoll_create(256);
    if (epfd == -1) {
      return false;
    }
    struct epoll_event ev;
    ev.data.fd = broadcastSocket;
    ev.events = (EPOLLIN | EPOLLOUT);
    epoll_ctl(epfd, EPOLL_CTL_ADD, broadcastSocket, &ev);
    ev.data.fd = tcpSocket;
    ev.events = (EPOLLIN | EPOLLOUT);
    epoll_ctl(epfd, EPOLL_CTL_ADD, tcpSocket, &ev);
    return true;
  }

  void epollRemove(int socket) {
    struct epoll_event ev;
    ev.data.fd = socket;
    ev.events = (EPOLLIN | EPOLLOUT);
    epoll_ctl(epfd, EPOLL_CTL_DEL, socket, &ev);
  }

  void removePendingState(__gnu_cxx::hash_map<int, std::list<TransferState*>::iterator>::iterator& itr) {
    std::list<TransferState*>::iterator listItr = itr->second;
    TransferState* state = *listItr;
    pendingStateMap.erase(itr);
    epollRemove(state->socket);
    pendingStateList.erase(listItr);
    delete state;
  }

  void removePendingState(std::list<TransferState*>::iterator& itr) {
    TransferState* state = *itr;
    int socket = state->socket;
    pendingStateMap.erase(socket);
    epollRemove(socket);
    pendingStateList.erase(itr);
    delete state;
  }

  void removeTransferState(int socket) {
    removeTransferState(transferStateMap.find(socket));
  }

  void removeTransferState(__gnu_cxx::hash_map<int, std::list<TransferState*>::iterator>::iterator itr) {
    std::list<TransferState*>::iterator listItr = itr->second;
    TransferState* state = *listItr;
    int socket = state->socket;
    std::string& uri = state->uri;
    transferStateMap.erase(itr);
    epollRemove(socket);

    __gnu_cxx::hash_map<std::string, UriState*>::iterator uriItr = uriStates.find(uri);
    UriState* uriState = uriItr->second;
    if (--uriState->transferCount == 0) {
      uriStates.erase(uriItr);
      delete uriState;
    }

    transferStateList.erase(listItr);
    delete state;
  }

  void removeTransferState(std::list<TransferState*>::iterator& itr) {
    TransferState* state = *itr;
    int socket = state->socket;
    std::string& uri = state->uri;

    transferStateMap.erase(socket);
    epollRemove(socket);
    __gnu_cxx::hash_map<std::string, UriState*>::iterator uriItr = uriStates.find(uri);
    UriState* uriState = uriItr->second;
    if (--uriState->transferCount == 0) {
      uriStates.erase(uriItr);
      delete uriState;
    }

    transferStateList.erase(itr);
    delete state;
  }

  long genResId() {
    if (now != secs) {
      secs = now;
      inc = 0;
    } else {
      ++inc;
    }
    return (((long)hostId << 24) | (long)secs | (long)inc);
  }

  bool udpOutBufferFull() {
    return (transferBufferWritePos > sizeof(transferBuffer) - MAX_TRANSPACK_SIZE);
  }

  // TODO(junhaozhang): 使用双缓存减少一次memcpy
  void moveTransferBufferPosIfNeeded() {
    if (transferBufferReadPos >= (sizeof(transferBuffer) >> 1)) {
      memcpy(transferBuffer, transferBuffer + transferBufferReadPos, transferBufferWritePos - transferBufferReadPos);
      transferBufferWritePos -= transferBufferReadPos;
      transferBufferReadPos = 0;
    }
  }
};

extern ServerStates g_svStates;

#endif  // SERVER_STATES_HPP_
