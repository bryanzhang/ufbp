#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#include "ufbp_common.hpp"
#include "socket_util.hpp"

#define BROAD_CONTENT "hello,world!"

int main() {
  // create socket and bind.
  int inet_sock, so_broadcast = 1;
  struct sockaddr_in si_server, si_client;
  if ((inet_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Broadcast UDP created socket error");
    return 1;
  }
  if (setsockopt(inet_sock, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)), 0) {
    perror("Broadcast UDP set socket error");
    close(inet_sock);
    return 1;
  }

  si_server.sin_family = AF_INET;
  si_server.sin_port = htons(UFBP_SERVER_PORT);
  si_server.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(inet_sock, (struct sockaddr*)&si_server, sizeof(si_server)) == -1) {
    perror("Bind error");
    close(inet_sock);
    return 1;
  }
  setnonblocking(inet_sock);

  si_client.sin_family = AF_INET;
  si_client.sin_port = htons(UFBP_CLIENT_PORT);
  si_client.sin_addr.s_addr = htonl(-1);

  // create epoll and register.
  struct epoll_event ev, events[20];
  int epfd = epoll_create(256);
  ev.data.fd = inet_sock;
  ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
  epoll_ctl(epfd,EPOLL_CTL_ADD, inet_sock, &ev);


  int nfds;
  for (; ;) {
    nfds = epoll_wait(epfd, events, 20, 500);
    for (int i = 0; i < nfds; ++i) {
      if (events[i].events & EPOLLIN) {
        fprintf(stderr, "data in!\n");        
      } else if (events[i].events & EPOLLOUT) {
        /*
        if (sendto(inet_sock, BROAD_CONTENT, strlen(BROAD_CONTENT), 0, (struct sockaddr*)&si_client, sizeof(si_client)) < 0) {
          perror("Broadcast send error!");
          close(inet_sock);
          return 1;
        }
        printf("Send successfully!\n");
        close(inet_sock);
        return 0;
        */
      }
    }
  }

  return 0;
}
