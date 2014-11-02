#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

#define UDP_CLIENT_PORT 12307

int main(){
  int inet_sock, socklen, so_reuseaddr = 1;
  char data[128];
  char buff[128];
  struct sockaddr_in addr, from;

  socklen = sizeof(addr);

  // establish socket
  if ((inet_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("Listen UDP created socket error!");
  }

  if (setsockopt(inet_sock, SOL_SOCKET, SO_REUSEADDR, &so_reuseaddr, sizeof(so_reuseaddr)) < 0) {
    perror("Listen UDP set socket error");
    close(inet_sock);
    return 1;
  }

  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  addr.sin_port = htons(UDP_CLIENT_PORT);
  addr.sin_family = AF_INET;

  if (bind(inet_sock, (struct sockaddr*)&addr, sizeof addr) < 0) {
    perror("Listen UDP bind server error!");
    close(inet_sock);
    return 2;
  }
  int len;
  for (; ;) {
    len = recvfrom(inet_sock, data, 127, 0, (struct sockaddr*)&from, (socklen_t*)&socklen);
    if (len < 0) {
      perror("Listen UDP send error!");
      close(inet_sock);
      return 3;
    } else {
      // getpeermac(inet_sock, buff);
      data[len] = '\0';
      // printf("Receive from %s:%d: %s\n\r", inet_ntoa(from.sin_addr), ntohs(from.sin_port), data);
      printf("Receive data %s\n", data);
      memset(data, 0, 128);
    }
    memset(&from, 0, sizeof(from));
  }
  close(inet_sock);
  return 0;
}


