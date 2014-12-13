#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <errno.h>

#include "ufbp_common.hpp"
#include "socket_util.hpp"
#include "server_states.hpp"
#include "reqpack.hpp"

int recvData(int socket) {
  struct sockaddr_in from;
  int socklen;
  int remaining = sizeof(g_svStates.buffer);
  unsigned char* pos = g_svStates.buffer;
  while (remaining > 0) {
    int ret = recvfrom(socket, pos, sizeof(g_svStates.buffer), 0,
      (struct sockaddr*)&from, (socklen_t*)&socklen);
    if (ret <= 0) {
      break;
    }  else if (ret >= sizeof(PackHeader)) {
      remaining -= ret;
      BufferUnit unit = { pos, ret };
      g_svStates.bufQueue.push(unit);
      pos += ret;
    }
  }
  return pos - g_svStates.buffer;
}

void handleData() {
  // TODO(junhaozhang): process in multi-threads.
  while (!g_svStates.bufQueue.empty()) {
    BufferUnit unit = g_svStates.bufQueue.front();
    g_svStates.bufQueue.pop();
    PackHeader* pack = (PackHeader*)unit.pos;
    if (pack->len != unit.len) {
      fprintf(stderr, "WARNING: pack len %d != bufferlen %d\n", pack->len, unit.len);
      continue;
    }
    if (pack->len < sizeof(PackHeader)) {
      fprintf(stderr, "WARNING: pack len %d < %d\n", pack->len, sizeof(PackHeader));
      continue;
    }

    int len = pack->len - sizeof(PackHeader);
    if (!memcmp(pack->type, REQPACK_TYPE, PACKTYPE_LENGTH)) {
      if (len < sizeof(ReqPackHeader)) {
        fprintf(stderr, "WARNING: req pack len %d < %d\n", len, sizeof(ReqPackHeader));
        continue;
      }
      ReqPackHeader* req = (ReqPackHeader*)(pack + 1);
      len -= sizeof(ReqPackHeader);
      fprintf(stderr, "recv req: %d, %*s\n", req->reqId, len, (char*)(req + 1));
      PullRequest pq = { req->reqId, (char*)(req + 1), len };
      g_svStates.reqQueue.push(pq);
    } else {
      // TODO(junhaozhang):
    }
  } 
}

void putReqsToSchedul() {
  while (!g_svStates.reqQueue.empty()) {
    PullRequest pq = g_svStates.reqQueue.front();
    g_svStates.reqQueue.pop();

    // g_svStates.reqScheduler.addRequest(pq);
  }
}

void cron() {
  // removeInactiveReqs();
  // putReqsToSchedule();
  // putChunksToSchedule();  // chunk调度队列会设置最大长度,防止remove时操作过多(现在是线性扫描)

  // 缓冲区没满都可以发送
  // unsigned char* pos = g_svStates.outBuffer + g_svStates.outBufferPos;
  // unsigned char* endPos = g_svStates.outBuffer + sizeof(g_svStates.outBuffer);
  // while (pos < endPos) {
    // Chunk chunk = g_svStates.chunkScheduler.schedule();
    
  // }
}

int main() {
  // create udp socket and bind.
  int so_broadcast = 1;
  struct sockaddr_in si_server, si_client;
  if ((g_svStates.socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Broadcast UDP created socket error");
    return 1;
  }
  if (setsockopt(g_svStates.socket, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)), 0) {
    perror("Broadcast UDP set socket error");
    close(g_svStates.socket);
    return 1;
  }

  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(UFBP_SERVER_PORT);
  si_server.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_svStates.socket, (struct sockaddr*)&si_server, sizeof(si_server)) == -1) {
    perror("Bind error");
    close(g_svStates.socket);
    return 1;
  }
  setnonblocking(g_svStates.socket);

  // create tcp socket and bind.
  if ((g_svStates.tcpSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("TCP server socket created socket error");
    return 1;
  }
  int opt = SO_REUSEADDR;
  if (setsockopt(g_svStates.tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
    perror("TCP server set socket error");
    close(g_svStates.tcpSocket);
    return 1;
  }

  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(UFBP_SERVER_TCP_PORT);
  si_server.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(g_svStates.tcpSocket, (struct sockaddr*)&si_server, sizeof(si_server)) == -1) {
    perror("Bind tcp port error");
    close(g_svStates.tcpSocket);
    return 1;
  }
  if (listen(g_svStates.tcpSocket, 128) == -1) {
    perror("Listen error");
    close(g_svStates.tcpSocket);
    return 1;
  }

  si_client.sin_family = AF_INET;
  si_client.sin_port = htons(UFBP_CLIENT_PORT);
  si_client.sin_addr.s_addr = htonl(-1);

  // create epoll and register.
  struct epoll_event ev, events[20];
  int epfd = epoll_create(256);
  ev.data.fd = g_svStates.socket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, g_svStates.socket, &ev);
  ev.data.fd = g_svStates.tcpSocket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, g_svStates.tcpSocket, &ev);

  int nfds;
  for (; ;) {
    nfds = epoll_wait(epfd, events, 20, 500);
    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == g_svStates.tcpSocket) {
        struct sockaddr_in local;
        int len = sizeof(sockaddr_in);
        int socket = accept(g_svStates.tcpSocket, (struct sockaddr*)&local, (socklen_t*)&len);
        if (socket == -1) {
          perror("Accept failure");
        } else {
          fprintf(stderr, "Accept!%d\n", socket);
          setnonblocking(socket);
          epoll_ctl(epfd, EPOLL_CTL_ADD, socket, &ev);
        }
        continue;
      }

      if (events[i].data.fd == g_svStates.socket) {
        continue;
      }

      if (events[i].events & EPOLLIN) {
        int len = recvData(g_svStates.socket);
        if (len > 0) {
          handleData();
        }
      } else if (events[i].events & EPOLLOUT) {
        /*
        if (sendto(g_svStates.socket, BROAD_CONTENT, strlen(BROAD_CONTENT), 0, (struct sockaddr*)&si_client, sizeof(si_client)) < 0) {
          perror("Broadcast send error!");
          close(g_svStates.socket);
          return 1;
        }
        printf("Send successfully!\n");
        close(g_svStates.socket);
        return 0;
        */
      }
      cron();
    }
  }

  return 0;
}
