#ifndef FILEINFO_HPP_
#define FILEINFO_HPP_

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct FileInfo {
  bool exists;
  unsigned long fileLength;
  unsigned long lastModifiedDate;
};

inline void fileinfo_init(FileInfo& fi) {
  fi.exists = false;
  fi.fileLength = 100;
  fi.lastModifiedDate = 100;
}

inline void getFileInfo(char* uri, int len, FileInfo& fi) {
  struct stat buf;
  fileinfo_init(fi);

  if (stat(uri, &buf) == -1 || (buf.st_mode & S_IFMT) != S_IFREG) {
    return;
  }

  fi.exists = true;
  fi.fileLength = buf.st_size;
  fi.lastModifiedDate = buf.st_mtime;
}

#endif  // FILEINFO_HPP_
