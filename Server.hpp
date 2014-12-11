/*
Neal Patel (nap7jz)
12/10/2014
Server.hpp: class for a FTP server
*/
#ifndef SERVER_HPP
#define SERVER_HPP 1

#include <cstdint>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <algorithm>
#include "Session.hpp"

class Server {
public:
  Server(uint16_t port_) : sct(-1), port(port_) {}

  // bind and listen
  bool initialize() {
    this->sct = socket(AF_INET, SOCK_STREAM, 0);

    if (this->sct == -1) {
      return false;
    }

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(this->port);
    addr.sin_addr.s_addr = INADDR_ANY;
    std::fill(addr.sin_zero, addr.sin_zero + sizeof(addr.sin_zero), 0);

    if (bind(this->sct, (sockaddr*)&addr, sizeof(sockaddr_in)) == -1) {
      return false;
    }

    return (listen(this->sct, SOMAXCONN) != 1);
  }

  // accept connections on a single thread (this method never returns)
  void start() {
    for (;;)
    {
      sockaddr_in sender;
      socklen_t len = sizeof(sockaddr_in);
      int fd = accept(this->sct, (sockaddr*)&sender, &len);

      if (fd != -1) {
	Session::create_session(fd, sender);
      }
    }
  }

  // cleanup
  virtual ~Server() {
    if (this->sct != -1) {
      close(this->sct);
    }
  }

private:
  int sct;
  uint16_t port;
};

#endif
