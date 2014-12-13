#include <arpa/inet.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "ufbp_common.hpp"
#include "socket_util.hpp"
#include "reqpack.hpp"

using namespace std;

void usage() {
  printf("Usage: ufbp_client ${host} ${uri} ${output_path}\n");
}

enum DownloadStatus {
  INIT,
  WAITFOR_RESP,
};

// states
DownloadStatus status = INIT;
unsigned char* buffer = new unsigned char[BUFFER_SIZE];

/*
bool sendRequestPacket(int socket, char* host, char* uri) {
  int totalLen = reqpack_init((PackHeader*)buffer, uri);
  int ret;
  unsigned char* pos = buffer;
  struct sockaddr_in si_server;
  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(UFBP_SERVER_PORT);
  // TODO(junhaozhang): support name resolve.
  si_server.sin_addr.s_addr = inet_addr(host);
  for (; ;) {
    ret = sendto(socket, pos, totalLen, 0, (struct sockaddr*)&si_server, sizeof(si_server));
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      }
      return false;
    } else if (ret != totalLen) {
      return false;
    }
    return true;
  }
}
*/
bool sendRequestPacket(int socket, char* uri) {
  int ret;
  unsigned char* pos = buffer;
  int remaining = reqpack_init((PackHeader*)buffer, uri);
  while (remaining > 0) {
    ret = write(socket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        return false;
      }
    }
    pos += ret;
    remaining -= ret;
  }
  return true;
}

int main(int argc, char** argv) {
  int inet_sock, socklen, so_reuseaddr = 1;
  struct sockaddr_in addr, from;
  char data[1024 * 1024];

  if (argc < 4) {
    usage();
    return 1;
  }

  char* host = argv[1];
  char* uri = argv[2];
  char* savepath = argv[3];

  // establish socket.
  if ((inet_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Listen UDP created socket error!");
  }

  if (setsockopt(inet_sock, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) {
    perror("Listen UDP set socket error");
    close(inet_sock);
    return 1;
  }

  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(UFBP_CLIENT_PORT);
  addr.sin_family = AF_INET;

  if (bind(inet_sock, (struct sockaddr*)&addr, sizeof addr) < 0) {
    perror("Listen UDP bind server error!");
    close(inet_sock);
    return 2;
  }
  fprintf(stderr, "UDP socket=%d\n", inet_sock);
  setnonblocking(inet_sock);

  // create epoll and register.
  struct epoll_event ev, events[20];
  memset(events, 0, sizeof(events));

  int epfd = epoll_create(256);
  ev.data.fd = inet_sock;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, inet_sock, &ev);

  // establish tcp socket.
  int tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (tcpSocket == -1) {
    perror("TCP client socket created error");
    return 1;
  }

  addr.sin_family = AF_INET;
  addr.sin_port = htons(UFBP_SERVER_TCP_PORT);
  addr.sin_addr.s_addr = inet_addr(host);

  if (connect(tcpSocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("Connect error");
    close(tcpSocket);
    return 1;
  }
  fprintf(stderr, "TCP socket connected!socket=%d\n", tcpSocket);
  setnonblocking(tcpSocket);
  ev.data.fd = tcpSocket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, tcpSocket, &ev);

  int nfds;
  int len;

  for (; ;) {
    nfds = epoll_wait(epfd, events, 20, 500);
    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == tcpSocket) {
        if (events[i].events & EPOLLOUT) {
          if (status == INIT) {
            if (!sendRequestPacket(tcpSocket, uri)) {
              perror("Send request packet failed");
              return 1;
            }
            fprintf(stderr, "Request send out, waiting for response!\n");
            status = WAITFOR_RESP;
          }
        }
        continue;
      }
    }
  }
  close(inet_sock);
  return 0;
}
