#ifndef TCP_SOCKET_STATE_HPP_
#define TCP_SOCKET_STATE_HPP_

struct TcpSocketState {
  unsigned char buffer[128 * 1024];
  unsigned char outBuffer[1024];  // TODO(junhaozhang): should use a singleton.
  int socket;
  int state;
  int bufferPos;
  int readPos;
};

#endif  // TCP_SOCKET_STATE_HPP_
