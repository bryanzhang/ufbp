#ifndef ACKPACK_HPP_
#define ACKPACK_HPP_

#pragma pack(push)
#pragma pack(1)
struct AckPackHeader {
  unsigned short count;
};
#pragma pack(pop)

#endif  // ACKPACK_HPP_
