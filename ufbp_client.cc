#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "ufbp_common.hpp"

using namespace std;

int main(int argc, char** argv) {
  int inet_sock, socklen, so_reuseaddr = 1;
  struct sockaddr_in addr, from;
  char data[1024 * 1024];

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

  /*
  // set non blocking.
  int flag = fcntl(inet_sock, F_GETFL, 0);
  flag |= O_NONBLOCK;
  if (fcntl(inet_sock, F_SETFL, flag) < 0) {
    perror("Fail to set non blocking!");
    return 3;
  }
  */

  int len;
  for (; ;) {
    len = recvfrom(inet_sock, data, 127, 0, (struct sockaddr*)&from, (socklen_t*)&socklen);
    if (len < 0) {
      perror("Listen UDP send error!");
      close(inet_sock);
      return 5;
    } else {
      data[len] = '\0';
      printf("Receive data %s\n", data);
      memset(data, 0, 128);
    }
    memset(&from, 0, sizeof(from));
  }
  close(inet_sock);
  return 0;
}
