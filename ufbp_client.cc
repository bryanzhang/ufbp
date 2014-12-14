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
#include "respack.hpp"

using namespace std;

void usage() {
  printf("Usage: ufbp_client ${host} ${uri} ${output_path}\n");
}

enum DownloadStatus {
  INIT,
  WAITFOR_RESP,
};
int epfd = -1;
int tcpSocket = -1;
unsigned char tcpInputBuffer[128 * 1024];
int tcpInputBufferPos = 0;
int tcpInputBufferReadPos = 0;
unsigned char tcpOutputBuffer[2048];

// states
DownloadStatus status = INIT;
unsigned char* buffer = new unsigned char[BUFFER_SIZE];

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
    } else if (ret == 0) {
      return false;
    }

    pos += ret;
    remaining -= ret;
  }
  return true;
}

bool handleTcpPacket(PackHeader* pack) {
  if (status == WAITFOR_RESP) {
    fprintf(stderr, "Pack len:%d\n", (int)pack->len);
    int len = pack->len - sizeof(PackHeader);
    if (!memcmp(pack->type, RESPACK_TYPE, PACKTYPE_LENGTH)) {
      if (len < sizeof(ResPackHeader)) {
        fprintf(stderr, "WARNING: res pack len %d < %d\n", len, sizeof(ResPackHeader));
        return false;
      }
   
      ResPackHeader* res = (ResPackHeader*)(pack + 1);
      len -= sizeof(ResPackHeader);
      fprintf(stderr, "Response: code=%hd,resId=%lld,filelength=%lld,lastModifiedDate=%lld\n", res->code, res->resId, res->fileLength, res->lastModifiedDate);
      return true;
    } else {
      fprintf(stderr, "Unexpected pack type:%.*s\n", PACKTYPE_LENGTH, pack->type);
      return false;
    }
  } else {
    return false;
  }
}

bool recvTcpPacket() {
  unsigned char* pos = tcpInputBuffer + tcpInputBufferPos;
  int remaining = sizeof(tcpInputBuffer) - tcpInputBufferPos;
  int ret;
  while (remaining > 0) {
    ret = read(tcpSocket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        break;
      }
      perror("Read socket error");
      struct epoll_event ev;
      ev.data.fd = tcpSocket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(epfd, EPOLL_CTL_DEL, tcpSocket, &ev);
      close(tcpSocket);
      return false;
    } else if (ret == 0) {
      fprintf(stderr, "Remote socket closed!\n");
      struct epoll_event ev;
      ev.data.fd = tcpSocket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(epfd, EPOLL_CTL_DEL, tcpSocket, &ev);
      close(tcpSocket);
      return false;
    } else {
      fprintf(stderr, "Recv %d bytes.\n", ret);
      pos += ret;
      remaining -= ret;
    }
  }
  tcpInputBufferPos = pos - tcpInputBuffer;

  for (; ;) {
    if (tcpInputBufferPos - tcpInputBufferReadPos < sizeof(PackHeader)) {
      break;
    }

    PackHeader* header = (PackHeader*)(tcpInputBuffer + tcpInputBufferReadPos);
    if (tcpInputBufferPos - tcpInputBufferReadPos < header->len) {
      break;
    }

    if (header->len < sizeof(PackHeader)) {
      fprintf(stderr, "Packet error!len %hd < PackHeader(%d)\n", header->len, sizeof(PackHeader));
      struct epoll_event ev;
      ev.data.fd = tcpSocket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(epfd, EPOLL_CTL_DEL, tcpSocket, &ev);
      close(tcpSocket);
      return false;
    }

    if (!handleTcpPacket(header)) {
      fprintf(stderr, "Packet error!\n");
      struct epoll_event ev;
      ev.data.fd = tcpSocket;
      ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
      epoll_ctl(epfd, EPOLL_CTL_DEL, tcpSocket, &ev);
      close(tcpSocket);
      return false;
    }

    tcpInputBufferReadPos += header->len;
  }
  if (tcpInputBufferReadPos >= sizeof(tcpInputBuffer) / 2) {
    memcpy(tcpInputBuffer, tcpInputBuffer + tcpInputBufferReadPos, tcpInputBufferPos - tcpInputBufferReadPos);
    tcpInputBufferPos -= tcpInputBufferReadPos;
    tcpInputBufferReadPos = 0;
  }
  return true;
}

int main(int argc, char** argv) {
  int inet_sock, socklen, so_reuseaddr = 1;
  struct sockaddr_in addr, from;
  char data[1025 * 1024];

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

  epfd = epoll_create(256);
  ev.data.fd = inet_sock;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, inet_sock, &ev);

  // establish tcp socket.
  tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
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
    if (nfds > 0) {
      fprintf(stderr, "NFDS:%d\n", nfds);
    }
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
        if (events[i].events & EPOLLIN) {
          recvTcpPacket();
        }
        continue;
      }
    }
  }
  close(inet_sock);
  return 0;
}
