#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "ufbp_common.hpp"
#include "socket_util.hpp"
#include "reqpack.hpp"
#include "respack.hpp"
#include "transpack.hpp"
#include "ackpack.hpp"
#include "client_states.hpp"

using namespace std;

void usage() {
  printf("Usage: ufbp_client ${host} ${uri} ${output_path}\n");
}

bool sendRequestPacket() {
  int ret;
  unsigned char* pos = g_cliStates.tempBuffer;
  int remaining = reqpack_init((PackHeader*)pos, (char*)g_cliStates.uri.c_str());
  while (remaining > 0) {
    ret = write(g_cliStates.tcpSocket, pos, remaining);
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
  g_cliStates.state = WAITFOR_RESP;
  fprintf(stderr, "Request sent!\n");
  return true;
}

bool handleResp() {
  // 需要能一次性接收完resp包,否则返回false
  unsigned char* pos = g_cliStates.tempBuffer;
  int remaining = sizeof(g_cliStates.tempBuffer);
  int ret;
  while (remaining > 0) {
    ret = read(g_cliStates.tcpSocket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        break;
      }
      perror("Read socket error");
      return false;
    } else if (ret == 0) {
      fprintf(stderr, "Remote socket closed!\n");
      return false;
    } else {
      pos += ret;
      remaining -= ret;
    }
  }

  PackHeader* pack = (PackHeader*)g_cliStates.tempBuffer;
  if (pos - g_cliStates.tempBuffer < sizeof(*pack)) {
    fprintf(stderr, "recved bytes %d < header len\n", pos - g_cliStates.tempBuffer);
    return false;
  }

  if (pack->len < sizeof(*pack)) {
    fprintf(stderr, "Packet error!len %hd < PackHeader(%d)\n", pack->len, sizeof(*pack));
    return false; 
  }

  int len = pack->len - sizeof(*pack);
  if (memcmp(pack->type, RESPACK_TYPE, PACKTYPE_LENGTH)) {
    return false;
  }

  if (len < sizeof(ResPackHeader)) {
    fprintf(stderr, "WARNING: res pack len %d < %d\n", len, sizeof(ResPackHeader));
    return false;
  }

  ResPackHeader* res = (ResPackHeader*)(pack + 1);
  len -= sizeof(ResPackHeader);
  if (res->code != 200) {
    return false;
  }
  g_cliStates.state = TRANSFER;
  g_cliStates.resId = res->resId;
  g_cliStates.fileLength = res->fileLength;
  g_cliStates.lastModifiedDate = res->lastModifiedDate;
  g_cliStates.chunks = (res->fileLength + CHUNK_SIZE - 1) / CHUNK_SIZE;
  if ((g_cliStates.fd = open((char*)(g_cliStates.path + ".tmp").c_str(), O_CREAT | O_RDWR | O_TRUNC, 0600)) == -1) {
    perror("open file error");
    return false;
  }
  ftruncate(g_cliStates.fd, res->fileLength);
  g_cliStates.mmap_addr = mmap(0, g_cliStates.fileLength, PROT_WRITE, MAP_SHARED, g_cliStates.fd, 0);
  if (g_cliStates.mmap_addr == MAP_FAILED) {
    perror("mmap error");
    return false;
  }
  int arrSize = ((g_cliStates.chunks + 31) >> 5);
  g_cliStates.recvMap = new unsigned int[arrSize];
  memset(g_cliStates.recvMap, 0, arrSize * sizeof(*g_cliStates.recvMap));
  g_cliStates.lastAckTime = g_cliStates.now;

  return true;
}

bool handleData() {
  int ret;
  struct sockaddr_in remoteAddr;
  socklen_t socklen = sizeof(remoteAddr);
  for (; ;) {
    ret = recvfrom(g_cliStates.udpSocket, g_cliStates.transferBuffer, sizeof(g_cliStates.transferBuffer), 0, (struct sockaddr*)&remoteAddr, &socklen);
    if (ret == -1) {
      if (errno == EAGAIN) {
        break;
      }
      perror("Recvfrom error");
      return false;
    }
    if (ret == 0) {
      fprintf(stderr, "Remote udp socket closed!\n");
      return false;
    }

    PackHeader* pack = (PackHeader*)g_cliStates.transferBuffer;
    if (ret < sizeof(*pack)) {
      fprintf(stderr, "pack len illegal,%d\n", ret);
      return false;
    }
    if (pack->len != ret) {
      fprintf(stderr, "pack len != ret,%d,%d\n", (int)pack->len, ret);
      return false;
    }

    if (memcmp(pack->type, TRANSPACK_TYPE, PACKTYPE_LENGTH)) {
      fprintf(stderr, "not a transpack!\n");
      return false;
    }

    int len = pack->len - sizeof(*pack);
    TransPackHeader* trans = (TransPackHeader*)(pack + 1);
    len -= sizeof(*trans);
    if (len < 0) {
      fprintf(stderr, "transpack is too short!\n");
      return false;
    }
    long pos = trans->chunk * (long)CHUNK_SIZE;
    long fileRemaining = (trans->fileLength - pos);
    int chunkSize = (fileRemaining <= (long)CHUNK_SIZE ? fileRemaining : CHUNK_SIZE);
    if (len != chunkSize) {
      fprintf(stderr, "chunkSize error!chunkSize=%d, len=%d\n", chunkSize, len);
      return false;
    }

    // debug(stderr, "resId: %lld, %lld\n", g_cliStates.resId, trans->resId);
    // debug(stderr, "fileLength: %lld, %lld\n", g_cliStates.fileLength, trans->fileLength);
    // debug(stderr, "lastModified: %lld, %lld\n", g_cliStates.lastModifiedDate, trans->lastModifiedDate);
    if (g_cliStates.resId != trans->resId || g_cliStates.fileLength != trans->fileLength || g_cliStates.lastModifiedDate != trans->lastModifiedDate) {
      debug(stderr, "not related!\n");
      continue;
    }

    if (trans->chunk < 0 || trans->chunk >= g_cliStates.chunks || g_cliStates.hasRecved(trans->chunk)) {
      debug(stderr, "illegal chunk or has recved!\n");
      continue;
    }

    debug(stderr, "trans recved: %d, size=%d\n", trans->chunk, g_cliStates.waitForAckChunks.size());
    // debug(stderr, "cpy\n");
    memcpy((char*)g_cliStates.mmap_addr + pos, trans + 1, chunkSize);
    g_cliStates.waitForAckChunks.push(trans->chunk);
    g_cliStates.setRecved(trans->chunk);
    ++g_cliStates.recvCount;
    debug(stderr, "recv: %d, %d\n", g_cliStates.recvCount, g_cliStates.chunks);
    if (g_cliStates.recvCount == g_cliStates.chunks) {
      g_cliStates.state = TRANSFER_FINISHED;
      debug(stderr, "right!\n");
      return true;
    }
  }
  return true;
}

// 返回剩余的字节数,-1表示出错
int sendAcks() {
  unsigned char* pos = g_cliStates.tcpOutBuffer + g_cliStates.tcpOutBufferReadPos;
  int remaining = g_cliStates.tcpOutBufferWritePos - g_cliStates.tcpOutBufferReadPos;
  int ret;
  while (remaining > 0) {
    debug(stderr, "Remaining: %d\n", remaining);
    ret = write(g_cliStates.tcpSocket, pos, remaining);
    if (ret == -1) {
      if (errno == EAGAIN) {
        return remaining;
      }
      return -1;
    } else if (ret == 0) {
      perror("send acks error");
      return -1;
    } else {
      debug(stderr, "Ret: %d\n", ret);
      pos += ret;
      remaining -= ret;
      g_cliStates.tcpOutBufferReadPos += ret;
      g_cliStates.lastAckTime = g_cliStates.now;
    }
  }
  g_cliStates.tcpOutBufferReadPos = g_cliStates.tcpOutBufferWritePos = 0;
  return 0;
}

bool packAndSendAcks(bool force) {
  // 先发送缓冲区中剩余字节,发不完直接返回
  int ret = sendAcks();
  if (ret == -1) {
    return false;
  } else if (ret > 0) {
    return true;
  }

  // 打包acks
  if (g_cliStates.waitForAckChunks.empty() || (!force && g_cliStates.lastAckTime + 500 > g_cliStates.now)) {
    return true;
  }

  unsigned short count = std::min((size_t)15 * 1024, g_cliStates.waitForAckChunks.size());
  unsigned int* p = ackpack_init((PackHeader*)g_cliStates.tcpOutBuffer, count);
  debug(stderr, "count: %d\n", count);
  while (count) {
    *p = g_cliStates.waitForAckChunks.front();
    g_cliStates.waitForAckChunks.pop();
    ++p;
    --count;
  }
  g_cliStates.tcpOutBufferWritePos = (unsigned char*)p - g_cliStates.tcpOutBuffer;
  debug(stderr, "tcpOutBufferWritePos = %d, try send acks!\n", g_cliStates.tcpOutBufferWritePos);
  // 再次尝试发送
  return (sendAcks() != -1);
}

void updateTime() {
  // TODO(junhaozhang): 墙面时间可能被修改,应该取milliseconds since startup.
  struct timeval localTime;
  gettimeofday(&localTime, NULL);

  g_cliStates.now = localTime.tv_sec * 1000 + (localTime.tv_usec / 1000);
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
  g_cliStates.uri = uri;
  g_cliStates.path = savepath;

  if (!g_cliStates.init(host)) {
    return 1;
  }

  int nfds;
  int len;

  for (; ;) {
    nfds = epoll_wait(g_cliStates.epfd, g_cliStates.events, 20, 10);
    updateTime();
    for (int i = 0; i < nfds; ++i) {
      if (g_cliStates.events[i].data.fd == g_cliStates.tcpSocket) {
        if (g_cliStates.events[i].events & EPOLLOUT) {
          if (g_cliStates.state == INIT) {
            if (!sendRequestPacket()) {
              return 1;
            }
          } else if (g_cliStates.state == TRANSFER) {
            if (!packAndSendAcks(false)) {
              return 1;
            }
          } else if (g_cliStates.state == TRANSFER_FINISHED) {
            debug(stderr, "Transfer finished!\n");
            if (!packAndSendAcks(true)) {
              fprintf(stderr, "WARNING: transfer finished,but the last ack pack not send!\n");
              g_cliStates.transferFinish();
              return 0;
            }
            debug(stderr, "packAndSendAcks finished!\n");
            if (g_cliStates.waitForAckChunks.empty() && g_cliStates.tcpOutBufferReadPos == g_cliStates.tcpOutBufferWritePos) {
              debug(stderr, "transfer finish begin!\n");
              g_cliStates.transferFinish();
              debug(stderr, "transfer finish end!\n");
              return 0;
            }
          }
        }

        if (g_cliStates.events[i].events & EPOLLIN) {
          if (g_cliStates.state == WAITFOR_RESP) {
            if (!handleResp()) {
              return 1;
            }
          }
        }
      } else if (g_cliStates.events[i].data.fd == g_cliStates.udpSocket) {
        if (g_cliStates.events[i].events & EPOLLIN) {
          if (g_cliStates.state == TRANSFER) {
            if (!handleData()) {
              return 1;
            }
          }
        }
      }
    }
  }
  return 0;  // never reach here.
}
