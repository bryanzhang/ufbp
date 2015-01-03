#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <errno.h>
#include <stdio.h>
#include <sys/time.h>

#include "ufbp_common.hpp"
#include "socket_util.hpp"
#include "server_states.hpp"
#include "reqpack.hpp"
#include "respack.hpp"
#include "transpack.hpp"
#include "ackpack.hpp"

bool sendRespPacket(int socket, unsigned char* buffer, unsigned short code, unsigned long resId, unsigned long fileLength, unsigned long lastModifiedDate) {
  int remaining = respack_init((PackHeader*)buffer, code, resId, fileLength, lastModifiedDate);
  unsigned char* pos = buffer;
  int ret;
  while (remaining > 0) {
    ret = write(socket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        continue;
      } else {
        perror("Send socket error");
        return false;
      }
    } else if (ret == 0) {
      fprintf(stderr, "remote Socket closed!\n");
      return false;
    } else {
      remaining -= ret;
      pos += ret;
    }
  }
  return true;
}

bool handleAckPacket(TransferState& state, AckPackHeader* ack, int len) {
  if (len < sizeof(AckPackHeader)) {
    fprintf(stderr, "WARNING: ack pack len %d < %d\n", len, sizeof(AckPackHeader));
    return false;
  }

  len -= sizeof(AckPackHeader);
  fprintf(stderr, "recv ack: %d\n", ack->count);
  if (len != sizeof(unsigned int) * ack->count) {
    fprintf(stderr, "WARNING: remain len %d != %d\n", len, sizeof(unsigned int) * ack->count);
    return false;
  }

  unsigned int* start = (unsigned int*)(ack + 1);
  unsigned int* end = start + ack->count;
  for (unsigned int* p = start; p < end; ++p) {
    state.acknowledge(*p);
  }
  if (state.finished()) {
    g_svStates.removeTransferState(state.socket);
  }
}

bool handleReqPacket(TransferState& state, ReqPackHeader* req, int len) {
  if (len < sizeof(ReqPackHeader)) {
    fprintf(stderr, "WARNING: req pack len %d < %d\n", len, sizeof(ReqPackHeader));
    return false;
  }

  len -= sizeof(ReqPackHeader);
  fprintf(stderr, "recv req: %*s\n", len, (char*)(req + 1));
  char* uri = (char*)(req + 1);
  --len;

  // uristate状态更新
  __gnu_cxx::hash_map<std::string, UriState*>::iterator uriItr = g_svStates.uriStates.find(uri);
  int code = 200;
  unsigned long resId = 0;
  long fileLength = 0;
  long lastModifiedDate = 0;
  int chunks = 0;
  if (uriItr != g_svStates.uriStates.end()) {
    UriState* state = uriItr->second;
    ++(state->transferCount);
    resId = state->resId;
    fileLength = state->fileLength;
    lastModifiedDate = state->lastModifiedDate;
    chunks = state->chunks;
  } else {
    // check file info, mmap, new uristate
    struct stat st;
    if (stat(uri, &st) == -1 || (st.st_mode & S_IFMT) != S_IFREG) {
      code = 404;
    } else {
      fileLength = st.st_size;
      lastModifiedDate = st.st_mtime;
      int fd = open(uri, O_RDWR, 0600);
      if (fd == -1) {
        perror("open file error");
        code = 500;
      } else {
        void* mmap_addr = mmap(0, fileLength, PROT_READ, MAP_PRIVATE | MAP_NORESERVE, fd, 0);
        if (mmap_addr == MAP_FAILED) {
          perror("mmap error");
          close(fd);
          code = 500;
        } else {
          resId = g_svStates.genResId();
          UriState* uriState = new UriState(uri, lastModifiedDate, fileLength, resId, fd, mmap_addr);
          debug(stderr, "fl=%lld,chunks=%d, %d\n", fileLength, uriState->chunks, (fileLength + CHUNK_SIZE - 1) / CHUNK_SIZE);
          chunks = uriState->chunks;
          g_svStates.uriStates[uri] = uriState;
        }
      }
    }
  }

  if (code == 200) {
    // 从pending列表/map删除，加入transfer列表/map
    __gnu_cxx::hash_map<int, std::list<TransferState*>::iterator>::iterator pendingItr = g_svStates.pendingStateMap.find(state.socket);
    g_svStates.pendingStateList.erase(pendingItr->second);
    g_svStates.pendingStateMap.erase(pendingItr);
    g_svStates.transferStateList.push_back(&state);
    g_svStates.transferStateMap[state.socket] = --g_svStates.transferStateList.end();

    // transfer state 状态变更
    state.chunks = chunks;
    debug(stderr, "chunks=%d\n", state.chunks);
    state.pending = false;
    state.uri = uri;
    int arrSize = ((chunks + 31) >> 5);
    state.acked = new unsigned int[arrSize];
    memset(state.acked, 0, sizeof(*state.acked) * arrSize);
  }

  return sendRespPacket(state.socket, g_svStates.tcpOutBuffer, code, resId, fileLength, lastModifiedDate) && code == 200;
}

bool handleTcpPacket(TransferState& state, PackHeader* pack) {
  fprintf(stderr, "Pack len:%d\n", (int)pack->len);
  int len = pack->len - sizeof(PackHeader);
  if (state.pending) {
    if (memcmp(pack->type, REQPACK_TYPE, PACKTYPE_LENGTH)) {
      fprintf(stderr, "Unexpected packet type:%.*s\n", PACKTYPE_LENGTH, pack->type);
      return false;
    }
    ReqPackHeader* req = (ReqPackHeader*)(pack + 1);
    return handleReqPacket(state, req, len);
  } else {
    if (memcmp(pack->type, ACKPACK_TYPE, PACKTYPE_LENGTH)) {
      fprintf(stderr, "Unexpected packet type:%.*s\n", PACKTYPE_LENGTH, pack->type);
      return false;
    }
    AckPackHeader* ack = (AckPackHeader*)(pack + 1);
    return handleAckPacket(state, ack, len);
  }
}

bool transfer() {
  while (!g_svStates.transferQueue.empty()) {
    debug(stderr, "Buffer:%d\n", g_svStates.transferQueue.size());
    unsigned short len = g_svStates.transferQueue.front();
    if (sendto(g_svStates.broadcastSocket, g_svStates.transferBuffer + g_svStates.transferBufferReadPos, len, 0, (struct sockaddr*)&g_svStates.siBroadcast, sizeof(g_svStates.siBroadcast)) == -1) {
      if (errno == EAGAIN) {
        break;
      } else {
        perror("Broadcast send error!");
        return false;
      }
    }
    g_svStates.transferQueue.pop();
    g_svStates.transferBufferReadPos += len;
  }

  g_svStates.moveTransferBufferPosIfNeeded();
  return true;
}

void prepareTransferPacket(UriState& uriState, int chunk) {
  long pos = CHUNK_SIZE * chunk;
  long remaining = (uriState.fileLength - pos);
  unsigned short len = (remaining >= CHUNK_SIZE ? CHUNK_SIZE : (unsigned short)remaining);

  PackHeader* pack = (PackHeader*)(g_svStates.transferBuffer + g_svStates.transferBufferWritePos);
  transpack_init(pack, uriState.resId, uriState.fileLength, uriState.lastModifiedDate, chunk, (char*)uriState.mmap_addr + pos, len);
  unsigned short packLen = pack->len;
  g_svStates.transferBufferWritePos += packLen;
  g_svStates.transferQueue.push(packLen);
}

void schedule() {
  if (g_svStates.udpOutBufferFull()) {
    return;
  }

  TransferState* state = NULL;
  UriState* uriState = NULL;
  ChunkState* chunk = NULL;
  std::list<ChunkState>::iterator curChunkItr;
  std::list<ChunkState>::iterator chunkItr;
  ChunkState c;
  for (std::list<TransferState*>::iterator itr = g_svStates.transferStateList.begin(); itr != g_svStates.transferStateList.end(); ++itr) {
    state = *itr;
    uriState = g_svStates.uriStates[state->uri];

    // 如果队列满了,选择发送在该队列中却不在全局队列中的chunk(如果网络可靠、队列长一些,走该分支可能性小一些,通常是的确丢了的包)
    if (state->waitForAckChunkMap.size() >= 5000) {
      debug(stderr, "hello\n");
      for (chunkItr = state->waitForAckChunkList.begin(); chunkItr != state->waitForAckChunkList.end();) {
        chunk = &(*chunkItr);
        if (uriState->recentSendChunkMap.find(chunk->pos) != uriState->recentSendChunkMap.end()) {
          ++chunkItr;
          continue;
        }
        prepareTransferPacket(*uriState, chunk->pos);
        chunk->lastSendTime = g_svStates.now;
        curChunkItr = chunkItr;
        ++chunkItr;
        state->waitForAckChunkList.erase(curChunkItr);
        state->waitForAckChunkList.push_back(*chunk);
        state->waitForAckChunkMap[chunk->pos] = (--state->waitForAckChunkList.end());
        uriState->recentSendChunkList.push_back(*chunk);
        uriState->recentSendChunkMap[chunk->pos] = (--uriState->recentSendChunkList.end());
        if (g_svStates.udpOutBufferFull()) {
          return;
        }
      }
    // 队列未满的情况下,sendPos继续往前走(没有ack的包)
    } else {
      debug(stderr, "hi, chunks=%d\n", state->chunks);
      while (state->sendPos < state->chunks) {
        debug(stderr, "hi1\n");
        if (state->hasAcked(state->sendPos)) {
          ++(state->sendPos);
          continue;
        }
        debug(stderr, "hi2\n");
        debug(stderr, "pointer=%lld, pos=%d\n", uriState, state->sendPos);
        prepareTransferPacket(*uriState, state->sendPos);
        debug(stderr, "hi3\n");
        c.pos = state->sendPos;
        c.lastSendTime = g_svStates.now;
        state->updateWaitForChunk(c);
        debug(stderr, "hi4\n");
        uriState->updateRecentSendChunk(c);
        debug(stderr, "hi5\n");
        ++(state->sendPos);
        if (g_svStates.udpOutBufferFull()) {
          return;
        }
      }
    }
  }
  debug(stderr, "schedule all right!\n");
}

void acceptConn() {
  struct sockaddr_in local;
  int len = sizeof(sockaddr_in);
  int socket = accept(g_svStates.tcpSocket, (struct sockaddr*)&local, (socklen_t*)&len);
  if (socket == -1) {
    perror("Accept failure");
  } else {
    fprintf(stderr, "Accept!%d\n", socket);
    setnonblocking(socket);
    struct epoll_event ev;
    ev.data.fd = socket;
    ev.events = (EPOLLIN | EPOLLOUT | EPOLLET);
    epoll_ctl(g_svStates.epfd, EPOLL_CTL_ADD, socket, &ev);

    TransferState* state = new TransferState(g_svStates.now, socket);
    g_svStates.pendingStateList.push_back(state);
    g_svStates.pendingStateMap[socket] = --g_svStates.pendingStateList.end();
  }
}

bool recvTcpData(TransferState& state) {
  unsigned char* pos = state.tcpInBuffer + state.tcpInBufferWritePos;
  int remaining = sizeof(state.tcpInBuffer) - state.tcpInBufferWritePos;
  int ret;
  while (remaining > 0) {
    ret = read(state.socket, pos, remaining);
    if (ret < 0) {
      if (errno == EAGAIN) {
        break;
      } else {
        perror("Read socket error");
        return false;
      }
    } else if (ret == 0) {
      fprintf(stderr, "Remote socket closed!\n");
      return false;
    } else {
      pos += ret;
      state.tcpInBufferWritePos += ret;
      remaining -= ret;
    }
  }
  state.tcpInBufferWritePos = pos - state.tcpInBuffer;
  return true;
}

void handleTcpInput(int socket) {
  __gnu_cxx::hash_map<int, std::list<TransferState*>::iterator>::iterator itr = g_svStates.pendingStateMap.find(socket);
  TransferState* state = NULL;
  if (itr == g_svStates.pendingStateMap.end()) {
    itr = g_svStates.transferStateMap.find(socket);
  }
  std::list<TransferState*>::iterator listItr = itr->second;
  state = *listItr;

  if (!recvTcpData(*state)) {
    if (state->pending) {
      g_svStates.removePendingState(itr);
    } else {
      g_svStates.removeTransferState(itr);
    }
    return;
  }

  // 更新最后接收时间,TransferState在链表中的位置
  state->lastAckTime = g_svStates.now;
  if (state->pending) {
    g_svStates.pendingStateList.erase(listItr);
    g_svStates.pendingStateList.push_back(state);
    itr->second = (--g_svStates.pendingStateList.end());
  } else {
    g_svStates.transferStateList.erase(listItr);
    g_svStates.transferStateList.push_back(state);
    itr->second = (--g_svStates.transferStateList.end());
  }

  for (; ;) {
    if (state->tcpInBufferWritePos - state->tcpInBufferReadPos < sizeof(PackHeader)) {
      break;
    }

    PackHeader* header = (PackHeader*)(state->tcpInBuffer + state->tcpInBufferReadPos); 
    if (state->tcpInBufferWritePos - state->tcpInBufferReadPos < header->len) {
      break;
    }

    if (!handleTcpPacket(*state, header)) {
      fprintf(stderr, "Exception occurs when handle input socket, socket=%d\n", state->socket);
      if (state->pending) {
        g_svStates.removePendingState(itr);
      } else {
        g_svStates.removeTransferState(itr);
      }
      return;
    }

    state->tcpInBufferReadPos += header->len;
  }
  state->moveBufferPosIfNeeded();
  debug(stderr, "all right!\n");
}

void updateTime() {
  // TODO(junhaozhang): 墙面时间可能被修改,应该取milliseconds since startup.
  struct timeval localTime;
  gettimeofday(&localTime, NULL);

  g_svStates.now = localTime.tv_sec * 1000 + (localTime.tv_usec / 1000);
}

void removeInactives() {
  // pending和transfer中时间超过5秒的要做处理
  TransferState* state = NULL;
  std::list<TransferState*>::iterator curItr;
  long expireTime = g_svStates.now - 5000;
  // debug(stderr, "removing inactives\n");
  for (std::list<TransferState*>::iterator itr = g_svStates.pendingStateList.begin(); itr != g_svStates.pendingStateList.end();) {
    state = *itr;
    if (state->lastAckTime > expireTime) {
      break;
    }
    curItr = itr;
    ++itr;
    g_svStates.removePendingState(curItr);
  }

  // debug(stderr, "removing 2\n");
  for (std::list<TransferState*>::iterator itr = g_svStates.transferStateList.begin(); itr != g_svStates.transferStateList.end();) {
    state = *itr;
    if (state->lastAckTime > expireTime) {
      break;
    }
    curItr = itr;
    ++itr;
    g_svStates.removeTransferState(curItr);
  }

  // debug(stderr, "removing 3\n");
  // 遍历所有uri,去掉其中recentSendChunkList中过期的chunk(2s)
  // TODO(junhao): 如果uri较多,有必要做一个按recent最老包时间升序排的队列
  expireTime += 3000;
  for (__gnu_cxx::hash_map<std::string, UriState*>::iterator itr = g_svStates.uriStates.begin(); itr != g_svStates.uriStates.end(); ++itr) {
    (itr->second)->cleanRecentSendChunks(expireTime);
  }
  // debug(stderr, "removing 4\n");
}

int main() {
  if (!g_svStates.init()) {
    return 1;
  }

  int nfds = 0;
  bool udpReady = false;
  for (; ;) {
    nfds = epoll_wait(g_svStates.epfd, g_svStates.events, 20, 1000);
    updateTime();
    for (int i = 0; i < nfds; ++i) {
      if (g_svStates.events[i].data.fd == g_svStates.tcpSocket) {
        acceptConn();
        continue;
      }

      if (g_svStates.events[i].data.fd == g_svStates.broadcastSocket) {
        if (g_svStates.events[i].events & EPOLLOUT) {
          udpReady = true;
        }
        continue;
      }

      if (g_svStates.events[i].events & EPOLLIN) {
        handleTcpInput(g_svStates.events[i].data.fd);
        continue;
      }
    }

    removeInactives();
    schedule();
    if (udpReady) {
      if (!transfer()) {
        return 1;
      }
    }
  }

  return 0;
}
