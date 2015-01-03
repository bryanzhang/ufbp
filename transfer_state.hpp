#ifndef TRANSFER_STATE_HPP_
#define TRANSFER_STATE_HPP_

#include <string>
#include <list>
#include <ext/hash_map>
#include <cstring>
#include "chunk_state.hpp"
#include "ufbp_common.hpp"

struct TransferState {
  int socket;
  unsigned int* acked;
  int sendPos;
  int chunks;
  long lastAckTime;  // 包的最后接收时间
  bool pending;
  std::string uri;

  std::list<ChunkState> waitForAckChunkList;
  __gnu_cxx::hash_map<int, std::list<ChunkState>::iterator> waitForAckChunkMap;

  unsigned char tcpInBuffer[BUFFER_SIZE];
  int tcpInBufferReadPos;
  int tcpInBufferWritePos;

  TransferState(long time, int s) : socket(s), acked(NULL), sendPos(0), chunks(0), lastAckTime(time), pending(true), 
      tcpInBufferReadPos(0), tcpInBufferWritePos(0) {
  }

  ~TransferState() {
    close(socket);
    delete acked;
  }

  // TODO(junhaozhang): 使用双缓存(ring buffer)减少一次memcpy
  void moveBufferPosIfNeeded() {
    if (tcpInBufferReadPos >= (sizeof(tcpInBuffer) >> 1)) {
      memcpy(tcpInBuffer, tcpInBuffer + tcpInBufferReadPos, tcpInBufferWritePos - tcpInBufferReadPos);
      tcpInBufferWritePos -= tcpInBufferReadPos;
      tcpInBufferReadPos = 0;
    }
  }

  void acknowledge(unsigned int chunk) {
    if (chunk >= chunks) {
      return;
    }

    acked[(chunk >> 5)] |= (1 << (chunk & 31));
    __gnu_cxx::hash_map<int, std::list<ChunkState>::iterator>::iterator itr = waitForAckChunkMap.find(chunk);
    if (itr == waitForAckChunkMap.end()) {
      return;
    }

    debug(stderr, "ack: %d,%d,%d\n", chunk, sendPos, waitForAckChunkMap.size());
    waitForAckChunkList.erase(itr->second);
    waitForAckChunkMap.erase(itr);
  }

  bool hasAcked(int chunk) {
    return ((acked[(chunk >> 5)] & (1 << (chunk & 31))) != 0);
  }

  bool finished() {
    return (sendPos == chunks && waitForAckChunkMap.empty());
  }

  void updateWaitForChunk(ChunkState& chunk) {
    __gnu_cxx::hash_map<int, std::list<ChunkState>::iterator>::iterator itr = waitForAckChunkMap.find(chunk.pos);
    waitForAckChunkList.push_back(chunk);
    if (itr != waitForAckChunkMap.end()) {
      waitForAckChunkList.erase(itr->second);
      itr->second = (--waitForAckChunkList.end());
    } else {
      waitForAckChunkMap[chunk.pos] = (--waitForAckChunkList.end());
    }
  }
};

#endif  // TRANSFER_STATE_HPP_
