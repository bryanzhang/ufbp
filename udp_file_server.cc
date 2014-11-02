#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#define BROAD_CONTENT "hello,world!"

#define UDP_CLIENT_PORT 12307

int main() {
  int inet_sock, so_broadcast = 1;
  struct sockaddr_in adr_bc;
  if ((inet_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Broadcast UDP created socket error");
  }
  if (setsockopt(inet_sock, SOL_SOCKET, SO_BROADCAST, &so_broadcast, sizeof(so_broadcast)) , 0) {
    perror("Broadcast UDP set socket error");
    close(inet_sock);
    return 1;
  }

  adr_bc.sin_family = AF_INET;
  adr_bc.sin_port = htons(UDP_CLIENT_PORT);
  adr_bc.sin_addr.s_addr = inet_addr("255.255.255.255");

  if (sendto(inet_sock, BROAD_CONTENT, strlen(BROAD_CONTENT), 0, (struct sockaddr*)&adr_bc, sizeof(adr_bc)) < 0) {
    perror("Broadcast send error!");
    close(inet_sock);
    return 2;
  }
  printf("Send successfully!\n");
  close(inet_sock);
  return 0;
}
