#ifndef ACKPACK_HPP_
#define ACKPACK_HPP_

#pragma pack(push)
#pragma pack(1)
struct AckPackHeader {
  unsigned short count;
};
#pragma pack(pop)

inline unsigned int* ackpack_init(PackHeader* pack, unsigned short count) {
  AckPackHeader* header = (AckPackHeader*)(pack + 1);
  header->count = count;
  memcpy(pack->type, ACKPACK_TYPE, PACKTYPE_LENGTH);
  pack->len = sizeof(*pack) + sizeof(*header) + count * sizeof(unsigned int);
  return (unsigned int*)(header + 1);
}

#endif  // ACKPACK_HPP_
