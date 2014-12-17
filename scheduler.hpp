#ifndef SCHEDULER_HPP_
#define SCHEDULER_HPP_

#include "fileinfo.hpp"

class Scheduler {
 public:
  bool exists(const char* uri) {
    return false;
  }

  void getFileInfo(FileInfo& fi) {
  }

  void addUri(const char* uri, FileInfo& fi, TcpSocketState& state) {
  }

  void addRequest(const char* uri, TcpSocketState& state) {
  }
};

#endif  // SCHEDULER_HPP_
