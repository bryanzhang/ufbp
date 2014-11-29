#ifndef REQPACK_H_
#define REQPACK_H_

#include "pack.hpp"

#define REQPACK_TYPE "PULL"

#pragma pack(push)
#pragma pack(1)
struct ReqPackHeader {
  unsigned long reqId;
};
#pragma pack(pop)

inline int reqpack_init(PackHeader* pack, char* uri) {
  // TODO(junhaozhang): req id set.
  // TODO(junhaozhang): network byte order.
  ReqPackHeader* header = (ReqPackHeader*)(pack + 1);
  header->reqId = 103;

  int len = strlen(uri);
  // TODO(junhaozhang): cksum current not used.
  pack->cksum = 0;
  return (pack->len = ((unsigned char*)memcpy(header + 1, uri, len) - (unsigned char*)pack) + len);
}

#endif // REQPACK_H_
