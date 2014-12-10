#ifndef SESSION_HPP
#define SESSION_HPP 1

#include <netdb.h>
#include <string>
#include <sstream>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>

class Session {
public:
  static void create_session(int fd, sockaddr_in sender) {
    Session sess(fd, sender);
    sess.interactive_prompt();
  }

private:
  void interactive_prompt() {
    // std::cout << "Welcome to the interactive prompt!" << std::endl;
    this->respond_with_code(220);

    while (this->running) {
      prompt_once();
    }
  }

  void prompt_once() {
    std::string line;

    while (true) {
      char tmp;
      int cnt = recv(this->fd, &tmp, 1, 0);

      if (cnt == -1) {
	printf("read() failed\n");
	this->running = false;
	return;
      } else if (cnt == 0) {
	printf("connection closed\n");
	this->running = false;
	return;
      }

      if (tmp == '\n') {
	break;
      }

      if (tmp != '\r') {
	line.push_back(tmp);
      }
    }

    if (line.length() >= 512) {
      respond_with_code(500);
      return;
    }

    std::string cmd;
    std::stringstream ss(line);
    std::getline(ss, cmd, ' ');

    printf("Interpreting command line: `%s`\n", line.c_str());

    if (cmd == "QUIT") {
      this->QUIT(cmd, ss);
    } else if (cmd == "USER") {
      this->USER(cmd, ss);
    } else if (cmd == "SYST") {
      this->SYST(cmd, ss);
    } else if (cmd == "PWD") {
      this->PWD(cmd, ss);
    } else if (cmd == "CWD") {
      this->CWD(cmd, ss);
    } else if (cmd == "TYPE") {
      this->TYPE(cmd, ss);
    } else if (cmd == "RMD") {
      this->RMD(cmd, ss);
    } else {
      this->respond_with_code(502);
      printf("Unknown command: %s\n", cmd.c_str());
    }
  }

  void respond_with(std::string msg, bool newline=true) {
    if (newline) {
      msg = msg + '\n';
    }

    write(this->fd, msg.c_str(), msg.length());
  }

  void respond_with_code(int code) {
    std::string msg = _msg_for_code(code);

    if (!msg.empty()) {
      respond_with(msg);
    }
  }

  std::string _msg_for_code(int code) {
    switch (code) {
    case 125:
      return "125 Data connection already open; transfer starting.";
    case 200:
      return "200 Command okay.";
    case 215:
      return "215 UNIX Type: L8";
    case 220:
      return "220 Welcome to my ftp server.";
    case 221:
      return "221 Service closing control connection.  Logged out if appropriate.";
    case 226:
      return "226 Closing data connection. Requested file action successful. ";
    case 230:
      return "230 User logged in, proceed.";
    case 250:
      return "Requested file action okay, completed.";
    case 257:
      return std::string("257 \"") + this->cwd + "\"";
    case 450:
      return "450 Requested file action not taken. File unavailable.";
    case 500:
      return "500 Syntax error, command unrecognized.";
    case 501:
      return "501 Syntax error in parameters or arguments.";
    case 502:
      return "502 Command not implemented.";
    case 503:
      return "503 Bad sequence of commands.";
    case 530:
      return "530 Not logged in.";
    case 550:
      return "550 Requested action not taken. File unavailable.";
    default:
      return std::string();
    }
  }

  bool QUIT(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    if (junk.empty()) {
      printf("Got a QUIT('%s')\n", cmd.c_str());

      this->running = false;
      respond_with_code(200);
      return true;
    } else {
      respond_with_code(501);
      return false;
    }
  }

  bool USER(std::string const& cmd, std::stringstream& ss) {
    std::string username;
    std::getline(ss, username);

    if (username.empty()) {
      respond_with_code(501);
      return false;
    } else if (username == "anonymous") {
      printf("Got a USER('%s', '%s')\n", cmd.c_str(), username.c_str());

      this->current_user = username;

      respond_with_code(230);
      return true;
    } else {
      printf("Got a USER('%s', '%s')\n", cmd.c_str(), username.c_str());

      respond_with_code(530);
      return false;
    }
  }

  bool SYST(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    if (junk.empty()) {
      printf("Got a SYST('%s')\n", cmd.c_str());

      respond_with_code(215);
      return true;
    } else {
      respond_with_code(501);
      return false;
    }
  }

  bool PWD(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    if (junk.empty()) {
      printf("Got a PWD('%s')\n", cmd.c_str());

      respond_with_code(257);
      return true;
    } else {
      respond_with_code(501);
      return false;
    }
  }

  bool CWD(std::string const& cmd, std::stringstream& ss) {
    std::string remote_dir;
    std::getline(ss, remote_dir);

    if (remote_dir.empty()) {
      respond_with_code(501);
      return false;
    } else {
      printf("Got a CWD('%s', '%s')\n", cmd.c_str(), remote_dir.c_str());

      if (this->_set_cwd(remote_dir)) {
	respond_with_code(250);
	return true;
      } else {
	respond_with_code(550);
	return false;
      }
    }
  }

  bool TYPE(std::string const& cmd, std::stringstream& ss) {
    std::string mode;
    std::getline(ss, mode);

    if (mode.empty()) {
      respond_with_code(501);
      return false;
    } else if (mode == "Image") {
      respond_with_code(200);
      return true;
    } else {
      // TODO: handle other modes?
      // nah, too much work
      respond_with_code(200);
      return false;
    }
  }

  bool RMD(std::string const& cmd, std::stringstream& ss) {
    std::string path;
    std::getline(ss, path);

    if (path.empty()) {
      respond_with_code(501);
      return false;
    } else {
      printf("Got a RMD('%s', '%s')\n", cmd.c_str(), path.c_str());

      if (this->_rmdir(path)) {
	respond_with_code(250);
	return true;
      } else {
	respond_with_code(550);
	return false;
      }
    }
  }

  // TODO: implement MKD, LIST, transaction shit


  std::string _get_cwd() const {
    char tmp[PATH_MAX + 1] { };
    getcwd(tmp, PATH_MAX + 1);
    return tmp;
  }

  bool _set_cwd(std::string const& path) {
    if (chdir(path.c_str()) == 0) {
      cwd = this->_get_cwd();
      return true;
    } else {
      return false;
    }
  }

  bool _rmdir(std::string const& path) const {
    return rmdir(path.c_str()) == 0;
  }

  Session(int fd_, sockaddr_in& sender_) :
    running(true), fd(fd_), sender(sender_) {
    this->_set_cwd("/tmp");
  }

  virtual ~Session() {
    if (this->fd != -1) {
      close(this->fd);
    }
  }

  bool running;
  int fd;
  sockaddr_in sender;
  std::string cwd;
  std::string current_user;
};

#endif
