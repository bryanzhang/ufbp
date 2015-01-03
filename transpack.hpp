#ifndef TRANSPACK_HPP_
#define TRANSPACK_HPP_

#include <cstring>
#include "pack.hpp"
#include "ufbp_common.hpp"

#pragma pack(push)
#pragma pack(1)
struct TransPackHeader {
  unsigned long resId;
  long fileLength;
  long lastModifiedDate;
  int chunk;
};
#pragma pack(pop)

inline int transpack_init(PackHeader* pack, unsigned long resId, long fileLength, long lastModifiedDate, int chunk, void* addr, unsigned short len) {
  TransPackHeader* header = (TransPackHeader*)(pack + 1);
  header->resId = resId;
  header->fileLength = fileLength;
  header->lastModifiedDate = lastModifiedDate;
  header->chunk = chunk;
  memcpy(pack->type, TRANSPACK_TYPE, PACKTYPE_LENGTH);
  memcpy(header + 1, addr, len);
  pack->len = sizeof(*header) + sizeof(*pack) + len;
  return pack->len;
}

#endif  // TRANSPACK_HPP_
