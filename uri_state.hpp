#ifndef URI_STATE_HPP_
#define URI_STATE_HPP_

#include <string>
#include <list>
#include <ext/hash_map>
#include <sys/mman.h>
#include "chunk_state.hpp"
#include "ufbp_common.hpp"

struct UriState {
  std::string uri;
  long lastModifiedDate;
  long fileLength;
  unsigned long resId;
  int chunks;
  int fd;
  void* mmap_addr;

  int transferCount;
  std::list<ChunkState> recentSendChunkList;  // 按最后发送时间排序
  __gnu_cxx::hash_map<int, std::list<ChunkState>::iterator> recentSendChunkMap;

  UriState(const char* u, long md, long fl, long id, int file, void* addr) : uri(u), lastModifiedDate(md), fileLength(fl), resId(id), chunks((fl + CHUNK_SIZE - 1) / CHUNK_SIZE), fd(file), mmap_addr(addr), transferCount(1) {
  }

  ~UriState() {
    close(fd);
    munmap(mmap_addr, fileLength);
  }

  void cleanRecentSendChunks(long expireTime) {
    std::list<ChunkState>::iterator curItr;
    for (std::list<ChunkState>::iterator itr = recentSendChunkList.begin(); itr != recentSendChunkList.end();) {
      if (itr->lastSendTime > expireTime) {
        break;
      }
      curItr = itr;
      ++itr;
      recentSendChunkList.erase(curItr);
      recentSendChunkMap.erase(itr->pos);
    }
  }

  void updateRecentSendChunk(ChunkState& chunk) {
    recentSendChunkList.push_back(chunk);
    __gnu_cxx::hash_map<int, std::list<ChunkState>::iterator>::iterator itr = recentSendChunkMap.find(chunk.pos);
    if (itr != recentSendChunkMap.end()) {
      recentSendChunkList.erase(itr->second);
      itr->second = (--recentSendChunkList.end());
    } else {
      recentSendChunkMap[chunk.pos] = (--(recentSendChunkList.end()));
    }
  }
};

#endif  // URI_STATE_HPP_
