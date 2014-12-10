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
    std::cout << "Welcome to the interactive prompt!" << std::endl;

    while (this->running) {
      prompt_once();
    }
  }

  void prompt_once() {
    std::string line;
    std::size_t line_len = 0;

    while (true) {
      char tmp;
      int cnt = read(this->fd, &tmp, 1);

      if (cnt <= 0) {
	respond_with_code(500);
	return;
      }

      if (tmp == '\n') {
	break;
      }

      line.push_back(tmp);
      line_len++;

      if (line_len >= 512) {
	respond_with_code(500);
	return;
      }
    }

    std::string cmd;
    std::stringstream ss(line);
    std::getline(ss, cmd, ' ');

    if (cmd == "QUIT") {
      this->QUIT(cmd, ss);
    } else if (cmd == "USER") {
      this->USER(cmd, ss);
    } else if (cmd == "PWD") {
      this->PWD(cmd, ss);
    } else if (cmd == "CWD") {
      this->CWD(cmd, ss);
    }
  }

  void respond_with_code(int code) {
    std::string msg = _msg_for_code(code);

    if (!msg.empty()) {
      msg = msg + '\n';
      write(this->fd, msg.c_str(), msg.length());
    }
  }

  std::string _msg_for_code(int code) {
    switch (code) {
    case 125:
      return "125 Data connection already open; transfer starting.";
    case 200:
      return "200 Command okay.";
    case 221:
      return "Service closing control connection.  Logged out if appropriate.";
    case 226:
      return "Closing data connection. Requested file action successful. ";
    case 230:
      return "User logged in, proceed.";
    case 250:
      return "Requested file action okay, completed.";
    case 257:
      return std::string("\"") + this->cwd + "\"";
    case 450:
      return "Requested file action not taken. File unavailable.";
    case 500:
      return "500 Syntax error, command unrecognized.";
    case 501:
      return "501 Syntax error in parameters or arguments.";
    case 502:
      return "Command not implemented.";
    case 503:
      return "Bad sequence of commands.";
    case 530:
      return "Not logged in.";
    case 550:
      return "Requested action not taken. File unavailable.";
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

      if (this->set_cwd(remote_dir)) {
	respond_with_code(250);
	return true;
      } else {
	respond_with_code(550);
	return false;
      }
    }
  }

  std::string _get_cwd() {
    char tmp[PATH_MAX + 1] { };
    getcwd(tmp, PATH_MAX + 1);
    return tmp;
  }

  bool set_cwd(std::string const& path) {
    if (chdir(path.c_str()) == 0) {
      cwd = _get_cwd();
      return true;
    } else {
      return false;
    }
  }

  Session(int fd_, sockaddr_in& sender_) :
    running(true), fd(fd_), sender(sender_) {
    this->set_cwd("/tmp");
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
