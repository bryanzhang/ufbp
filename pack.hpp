#ifndef PACK_H_
#define PACK_H_

#pragma pack(push)
#pragma pack(1)
struct PackHeader {
  unsigned short len;
  unsigned short cksum;
};
#pragma pack(pop)

#endif  // REQPACK_H_
