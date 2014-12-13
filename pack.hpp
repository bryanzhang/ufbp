#ifndef PACK_H_
#define PACK_H_

#define REQPACK_TYPE "PULL"
#define RESPACK_TYPE "RESP"

#pragma pack(push)
#pragma pack(1)
struct PackHeader {
  unsigned short len;
  unsigned short cksum;
  char type[4];
};
#pragma pack(pop)

#endif  // REQPACK_H_
