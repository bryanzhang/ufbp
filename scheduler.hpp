#ifndef SCHEDULER_HPP_
#define SCHEDULER_HPP_

#include "fileinfo.hpp"
#include "tcp_socket_state.hpp"
#include "uri_state_queue.hpp"

struct Scheduler {
  UriStateQueue uriStateQueue;
  bool exists(char* uri) {
    UriState* uriState;
    return uriStateQueue.GetConstEntry(uri, &uriState);
  }

  void getFileInfo(char* uri, FileInfo& fi) {
    UriState* state;
    uriStateQueue.GetConstEntry(uri, &state);
    fi.exists = true;
    fi.fileLength = state->fileLength;
    fi.lastModifiedDate = state->lastModifiedDate;
  }

  void addUri(char* uri, FileInfo& fi, TcpSocketState& state) {
    long t = time(NULL);
    UriState* s = new UriState(uri, fi.fileLength, fi.lastModifiedDate, t, state.socket);
    uriStateQueue.PushEntry(uri, s);
  }

  void addRequest(char* uri, TcpSocketState& state) {
    UriState* s;
    uriStateQueue.PopEntry(uri, &s);
    long t = time(NULL);
    s->add(new TransferState(state.socket, s->chunks, t));
    uriStateQueue.PushEntry(uri, s);
  }

  UriState* schedule() {
    char* uri;
    UriState* state;    
    uriStateQueue.PopLRUEntry(&uri, &state);
    return state;
  }
};

#endif  // SCHEDULER_HPP_
