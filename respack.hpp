#ifndef RESPACK_HPP_
#define RESPACK_HPP_

#pragma pack(push)
#pragma pack(1)
struct ResPackHeader {
  unsigned short code;
  unsigned long resId;
  unsigned long fileLength;
  unsigned long lastModifiedDate;
};
#pragma pack(pop)

inline int respack_init(PackHeader* pack, unsigned short code, unsigned long resId, unsigned long fileLength, unsigned long lastModifiedDate) {
  ResPackHeader* header = (ResPackHeader*)(pack + 1);
  header->code = code;
  header->resId = resId;
  header->fileLength = fileLength;
  header->lastModifiedDate = lastModifiedDate;
  memcpy(pack->type, RESPACK_TYPE, PACKTYPE_LENGTH);
  pack->cksum = 0;
  pack->len = sizeof(*header) + sizeof(*pack);
  return pack->len;
}

#endif // RESPACK_HPP_
