#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <ctime>

#define UDP_SERVER_PORT 12306
#define UDP_CLIENT_PORT 12307

#pragma pack(1)
// 1036 bytes.
struct ServerResponsePack {
  short code;  // 2 bytes.
  int requestId;  // 4 bytes.
  int filelength;  // 4 bytes.
  short packNum;  // 2 bytes.
  char data[1024];  // 1024 bytes.
};
// to process simply, request pack is also 1032 bytes.
struct FileRequestPack {
  char uri[1036];
};
#pragma pack()

ServerResponsePack* sendRequest(int socket, char* host, char* uri, ServerResponsePack* pack) {
  int uriLen = strlen(uri);
  if (uriLen > sizeof(FileRequestPack)) {
    fprintf(stderr, "URI length %d > %ld.\n", uriLen, sizeof(FileRequestPack));
    return NULL;
  }
  struct sockaddr_in host_addr;
  host_addr.sin_family = AF_INET;
  host_addr.sin_port = htons(UDP_SERVER_PORT);
  host_addr.sin_addr.s_addr = inet_addr(host);

  time_t oldTime = time(NULL);
  printf("Sending request...\n");
  if (sendto(socket, uri, uriLen, 0, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0) {
    perror("File Request send error!");
    return NULL;
  }
  int len = 0;
  socklen_t socklen = 0;
  struct sockaddr_in from_addr;
  time_t lastTime = oldTime, curTime = 0;
  for (; ;) {
    curTime = time(NULL);
    if (curTime - oldTime >= 5) {
      return NULL;
    }
    if (curTime != lastTime) {
      printf("Sending request...\n");
      if (sendto(socket, uri, uriLen, 0, (struct sockaddr*)&host_addr, sizeof(host_addr)) < 0) {
        perror("File Request send error!");
        return NULL;
      }
      lastTime = curTime;
    }
    len = recvfrom(socket, pack, sizeof(*pack), 0, (struct sockaddr*)&from_addr, &socklen);
    // printf("Response length: %d\n", len);
    if (len < 0) {
      if (errno != EAGAIN) {
        perror("Listen UDP send error!");
        return NULL;
      }
    } else if (len > 0) {
      if (len == sizeof(ServerResponsePack)) {
        return pack;
      } else {
        printf("Response length: %d\n", len);
      }
    }
  }
}

void usage() {
  printf("Usage: ./udp_file_client ${host} ${uri} ${output_file}\n");
}

int main(int argc, char** argv){
  int inet_sock, socklen, so_reuseaddr = 1;
  char data[sizeof(ServerResponsePack)];
  struct sockaddr_in addr, from;

  // process command line arguments.
  if (argc < 4) {
    usage();
    return 127;
  }
  char* host = argv[1];
  char* uri = argv[2];
  char* output = argv[3];

  socklen = sizeof(addr);

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
  addr.sin_port = htons(UDP_CLIENT_PORT);
  addr.sin_family = AF_INET;

  if (bind(inet_sock, (struct sockaddr*)&addr, sizeof addr) < 0) {
    perror("Listen UDP bind server error!");
    close(inet_sock);
    return 2;
  }

  // set non blocking.
  int flag = fcntl(inet_sock, F_GETFL, 0);
  flag |= O_NONBLOCK;
  if (fcntl(inet_sock, F_SETFL, flag) < 0) {
    perror("Fail to set non blocking!");
    return 3;
  }

  // send request.
  ServerResponsePack* pack = sendRequest(inet_sock, host, uri, (ServerResponsePack*)data);
  if (pack == NULL) {
    perror("Fail to receive server response!");
    close(inet_sock);
    return 4;
  }

  printf("Response received!\n");
  int len;
  for (; ;) {
    len = recvfrom(inet_sock, data, 127, 0, (struct sockaddr*)&from, (socklen_t*)&socklen);
    if (len < 0) {
      perror("Listen UDP send error!");
      close(inet_sock);
      return 5;
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

