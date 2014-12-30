#ifndef PACK_H_
#define PACK_H_

#define REQPACK_TYPE "PULL"
#define RESPACK_TYPE "RESP"
#define TRANSPACK_TYPE "TRAN"
#define ACKPACK_TYPE "ACKN"

#pragma pack(push)
#pragma pack(1)
struct PackHeader {
  unsigned short len;
  char type[4];
};
#pragma pack(pop)

#endif  // REQPACK_H_
