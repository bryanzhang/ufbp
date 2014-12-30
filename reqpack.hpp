#ifndef REQPACK_H_
#define REQPACK_H_

#include <cstdlib>
#include "pack.hpp"

#pragma pack(push)
#pragma pack(1)
struct ReqPackHeader {
};
#pragma pack(pop)

inline int reqpack_init(PackHeader* pack, char* uri) {
  // TODO(junhaozhang): network byte order.
  ReqPackHeader* header = (ReqPackHeader*)(pack + 1);

  int len = strlen(uri);
  memcpy(pack->type, REQPACK_TYPE, PACKTYPE_LENGTH);
  len = ((unsigned char*)strcpy((char*)(header + 1), uri) - (unsigned char*)pack) + len + 1;
  if (len > 64 * 1024 - 29) {
    fprintf(stderr, "packet two long!");
    exit(1);
  }
  pack->len = len;
  return len;
}

#endif // REQPACK_H_
