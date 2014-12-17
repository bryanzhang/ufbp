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
#include "respack.hpp"
#include "fileinfo.hpp"

bool sendRespPacket(int socket, unsigned char* buffer, unsigned short code, unsigned long resId, unsigned long fileLength, unsigned long lastModifiedDate) {
  int remaining = respack_init((PackHeader*)buffer, code, resId, fileLength, lastModifiedDate);
  unsigned char* pos = buffer;
  int ret;
  while (remaining > 0) {
    ret = write(socket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        perror("Send socket error");
        return false;
      }
    } else if (ret == 0) {
      fprintf(stderr, "remote Socket closed!\n");
      return false;
    } else {
      remaining -= ret;
      pos += ret;
    }
  }
  return true;
}

bool handleTcpPacket(TcpSocketState& state, PackHeader* pack) {
  if (state.state == 0) {
    fprintf(stderr, "Pack len:%d\n", (int)pack->len);
    int len = pack->len - sizeof(PackHeader);
    if (!memcmp(pack->type, REQPACK_TYPE, PACKTYPE_LENGTH)) {
      if (len < sizeof(ReqPackHeader)) {
        fprintf(stderr, "WARNING: req pack len %d < %d\n", len, sizeof(ReqPackHeader));
        return false;
      }
      ReqPackHeader* req = (ReqPackHeader*)(pack + 1);
      len -= sizeof(ReqPackHeader);
      fprintf(stderr, "recv req: %d, %*s\n", req->reqId, len, (char*)(req + 1));
      char* uri = (char*)(req + 1);
      --len;

      // TODO(junhaozhang):马上生成resId,获取文件相关信息,发送回复
      if (!g_svStates.scheduler.exists(uri)) {
        FileInfo fi;
        getFileInfo((char*)(req + 1), len, fi);
        if (fi.exists) {
          if (!sendRespPacket(state.socket, state.outBuffer, 200, 1000, fi.fileLength, fi.lastModifiedDate)) {
            return false;
          }
          g_svStates.scheduler.addUri(uri, fi, state);
          state.state = 1;
          fprintf(stderr, "Response packet sent, state turns 1!\n");
        } else {
          sendRespPacket(state.socket, state.outBuffer, 404, 1000, fi.fileLength, fi.lastModifiedDate);
          return false;
        }
      } else {
        FileInfo fi;
        g_svStates.scheduler.getFileInfo(fi);
        if (!sendRespPacket(state.socket, state.outBuffer, 200, 1000, fi.fileLength, fi.lastModifiedDate)) {
          return false;
        }
        g_svStates.scheduler.addRequest(uri, state);
        state.state = 1;
        fprintf(stderr, "Response packet sent, state turns 1!\n");
      }
      state.state = 1;
      return true;
    }

    fprintf(stderr, "Unexpected packet type:%.*s\n", PACKTYPE_LENGTH, pack->type);
    return false;
  }
}

void recvTcpPacket(TcpSocketState& state) {
  unsigned char* pos = state.buffer + state.bufferPos;
  int remaining = sizeof(state.buffer) - state.bufferPos;
  int ret;
  while (remaining > 0) {
    ret = read(state.socket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        break;
      }
      perror("Read socket error");
      __gnu_cxx::hash_map<int, TcpSocketState*>::iterator itr = g_svStates.socketsPool.find(state.state);
      g_svStates.socketsPool.erase(itr);
      close(state.socket);
      struct epoll_event ev;
      ev.data.fd = state.socket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(g_svStates.epfd, EPOLL_CTL_DEL, state.socket, &ev);
      delete itr->second;
      return;
    } else if (ret == 0) {
      fprintf(stderr, "Remote socket closed!\n");
      __gnu_cxx::hash_map<int, TcpSocketState*>::iterator itr = g_svStates.socketsPool.find(state.state);
      g_svStates.socketsPool.erase(itr);
      struct epoll_event ev;
      ev.data.fd = state.socket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(g_svStates.epfd, EPOLL_CTL_DEL, state.socket, &ev);
      delete itr->second;
      close(state.socket);
      return;      
    } else {
      pos += ret;
      remaining -= ret;
    }
  }
  state.bufferPos = pos - state.buffer;

  for (; ;) {
    // 攒够1帧才处理
    if (state.bufferPos - state.readPos < sizeof(PackHeader)) {
      break;
    }

    PackHeader* header = (PackHeader*)(state.buffer + state.readPos); 
    if (state.bufferPos - state.readPos < header->len) {
      break;
    }

    if (!handleTcpPacket(state, header)) {
      fprintf(stderr, "Exception occurs when handle input socket, socket=%d\n", state.socket);
      __gnu_cxx::hash_map<int, TcpSocketState*>::iterator itr = g_svStates.socketsPool.find(state.socket);
      delete itr->second;
      g_svStates.socketsPool.erase(itr);
      return;
    }

    state.readPos += header->len;
  }
  if (state.readPos >= sizeof(state.buffer) / 2) {
    memcpy(state.buffer, state.buffer + state.readPos, state.bufferPos - state.readPos);
    state.bufferPos -= state.readPos;
    state.readPos = 0;
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
  g_svStates.epfd = epoll_create(256);
  struct epoll_event ev;
  ev.data.fd = g_svStates.socket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(g_svStates.epfd, EPOLL_CTL_ADD, g_svStates.socket, &ev);
  ev.data.fd = g_svStates.tcpSocket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(g_svStates.epfd, EPOLL_CTL_ADD, g_svStates.tcpSocket, &ev);

  int nfds;
  for (; ;) {
    nfds = epoll_wait(g_svStates.epfd, g_svStates.events, 20, 500);
    for (int i = 0; i < nfds; ++i) {
      if (g_svStates.events[i].data.fd == g_svStates.tcpSocket) {
        struct sockaddr_in local;
        int len = sizeof(sockaddr_in);
        int socket = accept(g_svStates.tcpSocket, (struct sockaddr*)&local, (socklen_t*)&len);
        if (socket == -1) {
          perror("Accept failure");
        } else {
          fprintf(stderr, "Accept!%d\n", socket);
          setnonblocking(socket);
          ev.data.fd = socket;
          ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
          epoll_ctl(g_svStates.epfd, EPOLL_CTL_ADD, socket, &ev);
          __gnu_cxx::hash_map<int, TcpSocketState*>::iterator itr = g_svStates.socketsPool.find(socket);
          if (itr != g_svStates.socketsPool.end()) {
            delete itr->second;
            g_svStates.socketsPool.erase(itr);
          }
          TcpSocketState* state = new TcpSocketState();
          state->socket = socket;
          state->state = 0;
          state->bufferPos = 0;
          state->readPos = 0;
          g_svStates.socketsPool[socket] = state;
        }
        continue;
      }

      if (g_svStates.events[i].data.fd == g_svStates.socket) {
        continue;
      }

/*
      if (g_svStates.socketsPool.find(socket) == g_svStates.socketsPool.end()) {
        continue;
      }
*/

      if (g_svStates.events[i].events & EPOLLIN) {
        recvTcpPacket(*g_svStates.socketsPool[g_svStates.events[i].data.fd]);
      } else if (g_svStates.events[i].events & EPOLLOUT) {
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
