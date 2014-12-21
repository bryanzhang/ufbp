#ifndef TRANSPACK_HPP_
#define TRANSPACK_HPP_

#pragma pack(push)
#pragma pack(1)
struct TransPackHeader {
  unsigned long resId;
  long fileLength;
  long lastModifiedDate;
  int chunk;
};
#pragma pack(pop)

inline int transpack_init(PackHeader* pack, unsigned long resId, long fileLength, long lastModifiedDate, int chunk, void* mmap_addr, unsigned short len) {
  TransPackHeader* header = (TransPackHeader*)(pack + 1);
  header->resId = resId;
  header->fileLength = fileLength;
  header->lastModifiedDate = lastModifiedDate;
  header->chunk = chunk;
  memcpy(pack->type, TRANSPACK_TYPE, PACKTYPE_LENGTH);
  memcpy(header + 1, mmap_addr, len);
  pack->cksum = 0;
  pack->len = sizeof(*header) + sizeof(*pack) + len;
  return pack->len;
}

#endif  // TRANSPACK_HPP_
