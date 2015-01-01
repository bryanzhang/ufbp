#ifndef CLIENT_STATES_HPP_
#define CLIENT_STATES_HPP_

#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <string>
#include <queue>
#include "ufbp_common.hpp"
#include "socket_util.hpp"

enum EClientState {
  INIT,
  WAITFOR_RESP,
  TRANSFER,
  TRANSFER_FINISHED,  // 接收完成,等待最后一个ack发送完成
  FINISH_ACK,  // 接收被服务器端确认
};

// tcp发送请求,接收response都使用tempBuffer(因为这两个都只使用一次),这两个都使用阻塞方式
// tcp发送ack包使用tcpOutBuffer,非阻塞方式(每个连接需要一个)
// 接收广播的文件数据,使用transferBuffer(全局共享)
struct ClientStates {
  // global data
  long now;
  int epfd;
  struct epoll_event events[20];
  unsigned char tempBuffer[65536];
  int udpSocket;
  unsigned char transferBuffer[BUFFER_SIZE];

  // per client
  EClientState state;
  int tcpSocket;
  unsigned char tcpOutBuffer[65536];  // 用于发送ack包
  int tcpOutBufferReadPos;
  int tcpOutBufferWritePos;

  unsigned long resId;
  std::string uri;
  std::string path;
  int fd;
  long lastModifiedDate;
  long fileLength;
  int chunks;
  void* mmap_addr;

  unsigned int* recvMap;
  int recvCount;

  std::queue<int> waitForAckChunks;
  long lastAckTime;

  inline ClientStates() : now(0), epfd(-1), udpSocket(-1),
    state(INIT), tcpSocket(-1), tcpOutBufferReadPos(0), tcpOutBufferWritePos(0),
    resId(0), fd(-1), lastModifiedDate(0), fileLength(0), chunks(0), mmap_addr(MAP_FAILED), recvMap(NULL), recvCount(0), lastAckTime(0) {
  }

  ~ClientStates() {
    close(epfd);
    close(udpSocket);
    close(tcpSocket);
    if (mmap_addr != MAP_FAILED) {
      munmap(mmap_addr, fileLength);
    }
    if (fd != -1) {
      close(fd);
      unlink((path + ".tmp").c_str());
    }
    delete recvMap;
  }

  void transferFinish() {
    close(tcpSocket);
    tcpSocket = -1;
    munmap(mmap_addr, fileLength);
    mmap_addr = MAP_FAILED;
    close(fd);
    fd = -1;
    rename((path + ".tmp").c_str(), path.c_str());
    delete recvMap;
  }

  bool initUdp() {
    if ((udpSocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("Listen UDP created socket error!");
      return false;
    }

    int so_reuseaddr = 1;
    if (setsockopt(udpSocket, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) {
      perror("Listen UDP set socket error");
      close(udpSocket);
      return false;
    }

    struct sockaddr_in addr;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(UFBP_CLIENT_PORT);
    addr.sin_family = AF_INET;
    if (bind(udpSocket, (struct sockaddr*)&addr, sizeof addr) < 0) {
      perror("Listen UDP bind server error!");
      close(udpSocket);
      return false;
    }
    setnonblocking(udpSocket);
    return true;
  }

  bool initTcp(char* host) {
    // establish tcp socket.
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket == -1) {
      perror("TCP client socket created error");
      return false;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UFBP_SERVER_TCP_PORT);
    addr.sin_addr.s_addr = inet_addr(host);

    if (connect(tcpSocket, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      perror("Connect error");
      close(tcpSocket);
      return false;
    }
    setnonblocking(tcpSocket);
    return true;
  }

  bool initEpoll() {
    epfd = epoll_create(256);
    struct epoll_event ev;
    ev.data.fd = udpSocket;
    ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
    epoll_ctl(epfd, EPOLL_CTL_ADD, udpSocket, &ev);

    ev.data.fd = tcpSocket;
    ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
    epoll_ctl(epfd, EPOLL_CTL_ADD, tcpSocket, &ev);
    return true;
  }

  inline bool init(char* host) {
    return initUdp() && initTcp(host) && initEpoll();
  }

  void setRecved(int chunk) {
    recvMap[(chunk >> 5)] |= (1 << (chunk & 31));
  }

  bool hasRecved(int chunk) {
    return (recvMap[(chunk >> 5)] & (1 << (chunk & 31)));
  }
};

extern ClientStates g_cliStates;

#endif  // CLIENT_STATES_HPP_
