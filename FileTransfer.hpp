#ifndef SESSION_HPP
#define SESSION_HPP 1

#include <netdb.h>

class FileTransfer {
public:
  FileTransfer(Session& parent_) : parent(parent_) {}

  virtual ~FileTransfer() {}

private:
  Session& parent;
};

#endif
