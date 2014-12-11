/*
Neal Patel (nap7jz)
12/10/2014
my_ftpd.cpp: FTP server program
*/
#include <iostream>
#include <cstdlib>
#include "Server.hpp"

void usage(char const* program_name) {
  std::cerr << "Usage: " << program_name << " <port>" << std::endl;
  std::cerr << "<port>: a valid and available port number" << std::endl;
  exit(1);
}

int main(int argc, char* argv[]) {
  char const* program_name = (argc >= 1 ? argv[0] : "my_ftpd");

  // parse command-line arg
  if (argc != 2) {
    usage(program_name);
  }

  int port = atoi(argv[1]);

  // validate command-line arg
  if (port < 1 || port > 65535) {
    usage(program_name);
  }

  // create a server object
  Server serv((uint16_t)port);

  // try to listen on the given port
  if (!serv.initialize()) {
    usage(program_name);
  }

  // accept incoming connections on a new thread, forever
  serv.start();

  // unreachable
  return 0;
}
