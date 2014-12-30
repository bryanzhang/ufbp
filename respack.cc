#include <cstring>
#include "respack.hpp"
#include "ufbp_common.hpp"

int respack_init(PackHeader* pack, unsigned short code, unsigned long resId, long fileLength, long lastModifiedDate) {
  ResPackHeader* header = (ResPackHeader*)(pack + 1);
  header->code = code;
  header->resId = resId;
  header->fileLength = fileLength;
  header->lastModifiedDate = lastModifiedDate;
  memcpy(pack->type, RESPACK_TYPE, PACKTYPE_LENGTH);
  pack->len = sizeof(*header) + sizeof(*pack);
  return pack->len;
}
