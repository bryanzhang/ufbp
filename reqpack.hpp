#ifndef REQPACK_H_
#define REQPACK_H_

#include <cstdlib>
#include "pack.hpp"

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
  memcpy(pack->type, REQPACK_TYPE, PACKTYPE_LENGTH);
  // TODO(junhaozhang): cksum current not used.
  pack->cksum = 0;
  len = ((unsigned char*)memcpy(header + 1, uri, len) - (unsigned char*)pack) + len;
  if (len > 64 * 1024 - 29) {
    fprintf(stderr, "packet two long!");
    exit(1);
  }
  pack->len = len;
  return len;
}

#endif // REQPACK_H_
