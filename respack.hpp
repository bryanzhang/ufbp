#ifndef RESPACK_HPP_
#define RESPACK_HPP_

#include "pack.hpp"

#pragma pack(push)
#pragma pack(1)
struct ResPackHeader {
  unsigned short code;
  unsigned long resId;
  long fileLength;
  long lastModifiedDate;
};
#pragma pack(pop)

int respack_init(PackHeader* pack, unsigned short code, unsigned long resId, long fileLength, long lastModifiedDate);

#endif // RESPACK_HPP_
