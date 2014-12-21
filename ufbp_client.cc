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
#include "transpack.hpp"

using namespace std;

void usage() {
  printf("Usage: ufbp_client ${host} ${uri} ${output_path}\n");
}

enum DownloadStatus {
  INIT,
  WAITFOR_RESP,
  WAITFOR_TRANS,
};
int udpSocket = -1;
int epfd = -1;
int tcpSocket = -1;
unsigned char tcpInputBuffer[128 * 1024];
int tcpInputBufferPos = 0;
int tcpInputBufferReadPos = 0;
unsigned char tcpOutputBuffer[2048];
unsigned char udpInputBuffer[MAX_TRANSPACK_SIZE];

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
      if (res->code != 200) {
        return false;
      }
      status = WAITFOR_TRANS;
      return true;
    } else {
      fprintf(stderr, "Unexpected pack type:%.*s\n", PACKTYPE_LENGTH, pack->type);
      return false;
    }
  } else {
    return false;
  }
}

bool handleUdpPacket(PackHeader* pack) {
  if (status != WAITFOR_TRANS) {
    return false;
  }

  if (memcmp(pack->type, TRANSPACK_TYPE, PACKTYPE_LENGTH)) {
    perror("not a transpack!");
    return false;
  }

  int len = pack->len - sizeof(*pack);
  TransPackHeader* trans = (TransPackHeader*)(pack + 1);
  len -= sizeof(*trans);
  if (len < 0) {
    perror("transpack is too short!");
    return false;
  }
  long fileRemaining = (trans->fileLength - trans->chunk * CHUNK_SIZE);
  int chunkSize = (fileRemaining <= CHUNK_SIZE ? fileRemaining : CHUNK_SIZE);
  if (len != chunkSize) {
    perror("chunkSize error!");
    return false;
  }

  fprintf(stderr, "resId=%lld,fileLength=%lld,lastModifiedDate=%lld,chunk=%d\n", trans->resId, trans->fileLength, trans->lastModifiedDate, trans->chunk);
  fprintf(stderr, "Chunk data:%.*s\n", chunkSize, (char*)(trans + 1));
  return true;
}

bool recvUdpPacket() {
  int ret;
  struct sockaddr_in remoteAddr;
  socklen_t socklen = sizeof(remoteAddr);
  for (; ;) {
    ret = recvfrom(udpSocket, udpInputBuffer, sizeof(udpInputBuffer), 0, (struct sockaddr*)&remoteAddr, &socklen);
    if (ret == -1) {
      if (errno == EAGAIN) {
        break;
      }
      perror("Recvfrom error");
      return false;
    }

    PackHeader* pack = (PackHeader*)udpInputBuffer;
    if (pack->len != ret) {
      perror("pack len != ret");
      return false;
    }

    if (pack->len < sizeof(*pack)) {
      perror("pack len too short!");
      return false;
    }


    return handleUdpPacket(pack);
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
  int socklen, so_reuseaddr = 1;
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
  if ((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Listen UDP created socket error!");
  }

  if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) {
    perror("Listen UDP set socket error");
    close(udpSocket);
    return 1;
  }

  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(UFBP_CLIENT_PORT);
  addr.sin_family = AF_INET;

  if (bind(udpSocket, (struct sockaddr*)&addr, sizeof addr) < 0) {
    perror("Listen UDP bind server error!");
    close(udpSocket);
    return 2;
  }
  fprintf(stderr, "UDP socket=%d\n", udpSocket);
  setnonblocking(udpSocket);

  // create epoll and register.
  struct epoll_event ev, events[20];
  memset(events, 0, sizeof(events));

  epfd = epoll_create(256);
  ev.data.fd = udpSocket;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd, EPOLL_CTL_ADD, udpSocket, &ev);

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
          if (!recvTcpPacket()) {
            fprintf(stderr, "Program exit!\n");
            return 1;
          }
        }
        continue;
      }

      if (events[i].data.fd == udpSocket) {
        if (events[i].events & EPOLLIN) {
          if (!recvUdpPacket()) {
            fprintf(stderr, "Program exit!\n");
            return 1;
          }
        }
        continue;
      }
    }
  }
  close(udpSocket);
  return 0;
}
