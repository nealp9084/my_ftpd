/*
Neal Patel (nap7jz)
12/10/2014
Session.hpp: class for a FTP server session
*/
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
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <fcntl.h>

// represents an FTP session
class Session {
public:
  // the only accessible method from outside: runs an FTP session
  static void create_session(int fd, sockaddr_in sender) {
    Session sess(fd, sender);
    sess.interactive_prompt();
  }

private:
  // start the session, and get input
  void interactive_prompt() {
    this->respond_with_code(220);

    while (this->running) {
      prompt_once();
    }
  }

  // get a line of input and process it
  void prompt_once() {
    std::string line;

    while (true) {
      char tmp;
      int cnt = recv(this->fd, &tmp, 1, 0);

      if (cnt == -1) {
	// read() failed
	this->running = false;
	return;
      } else if (cnt == 0) {
	// connection closed
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

    // input too long?
    if (line.length() >= 512) {
      respond_with_code(500);
      return;
    }

    std::string cmd;
    std::stringstream ss(line);
    std::getline(ss, cmd, ' ');

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
    } else if (cmd == "MODE") {
      this->MODE(cmd, ss);
    } else if (cmd == "STRU") {
      this->STRU(cmd, ss);
    } else if (cmd == "RMD") {
      this->RMD(cmd, ss);
    } else if (cmd == "MKD") {
      this->MKD(cmd, ss);
    } else if (cmd == "PORT") {
      this->PORT(cmd, ss);
    } else if (cmd == "LIST") {
      this->LIST(cmd, ss);
    } else if (cmd == "STOR") {
      this->STOR(cmd, ss);
    } else if (cmd == "RETR") {
      this->RETR(cmd, ss);
    } else {
      this->respond_with_code(502);
    }
  }

  // helper method: send a string to the client
  void respond_with(std::string msg, bool newline=true) const {
    if (newline) {
      msg = msg + '\n';
    }

    write(this->fd, msg.c_str(), msg.length());
  }

  // helper method: send an error string to the client, given the code
  void respond_with_code(int code) const {
    std::string msg = _msg_for_code(code);

    if (!msg.empty()) {
      respond_with(msg);
    }
  }

  // helper method: get the error string given the code
  std::string _msg_for_code(int code) const {
    switch (code) {
    case 125:
      return "125 Data connection already open; transfer starting.";
    case 150:
      return "150 Opening Binary mode data connection.";
    case 200:
      return "200 Command okay.";
    case 215:
      return "215 UNIX Type: L8";
    case 220:
      return "220 Welcome to my ftp server.";
    case 221:
      return "221 Service closing control connection.  Logged out if appropriate.";
    case 226:
      return "226 Transfer complete.";
    case 230:
      return "230 User logged in, proceed.";
    case 250:
      return "Requested file action okay, completed.";
    case 450:
      return "450 Requested file action not taken. File unavailable.";
    case 451:
      return "451 Requested action aborted: local error in processing.";
    case 500:
      return "500 Syntax error, command unrecognized.";
    case 501:
      return "501 Syntax error in parameters or arguments.";
    case 502:
      return "502 Command not implemented.";
    case 503:
      return "503 Bad sequence of commands.";
    case 504:
      return "504 Command not implemented for that parameter.";
    case 530:
      return "530 Not logged in.";
    case 550:
      return "550 Requested action not taken. File unavailable.";
    default:
      return std::string();
    }
  }

  // quit the FTP session
  bool QUIT(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    // bad # args?
    if (!junk.empty()) {
      respond_with_code(501);
      return false;
    }

    this->running = false;
    respond_with_code(200);
    return true;
  }

  // allow "anonymous", nobody else
  bool USER(std::string const& cmd, std::stringstream& ss) {
    std::string username;
    std::getline(ss, username);

    // bad # args?
    if (username.empty()) {
      respond_with_code(501);
      return false;
    }

    // only allow the "anonymous" user
    if (username == "anonymous") {
      // update session state
      this->current_user = username;
      this->logged_in = true;

      respond_with_code(230);
      return true;
    } else {
      respond_with_code(530);
      return false;
    }
  }

  // return a generic FTP server id string
  bool SYST(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    // bad # args?
    if (!junk.empty()) {
      respond_with_code(501);
      return false;
    }

    respond_with_code(215);
    return true;
  }

  // print working dir
  bool PWD(std::string const& cmd, std::stringstream& ss) {
    std::string junk;
    std::getline(ss, junk);

    // bad # args?
    if (!junk.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    respond_with(std::string("257 \"") + this->fakify(this->_get_cwd()) + "\"");
    return true;
  }

  // change working dir
  bool CWD(std::string const& cmd, std::stringstream& ss) {
    std::string remote_dir;
    std::getline(ss, remote_dir);

    // bad # args?
    if (remote_dir.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    if (this->_set_cwd(remote_dir)) {
      respond_with_code(250);
      return true;
    } else {
      respond_with_code(550);
      return false;
    }
  }

  // change transmission type
  bool TYPE(std::string const& cmd, std::stringstream& ss) {
    std::string type;
    std::getline(ss, type);

    // bad # args?
    if (type.length() != 1) {
      respond_with_code(501);
      return false;
    }

    // only handle the binary/image ("I") type for now
    // the other types are too much work, so just return 504
    if (type == "I") {
      // update session state
      this->current_type = type[0];

      respond_with("200 Switching to Binary mode.");
      return true;
    } else {
      respond_with_code(504);
      return false;
    }
  }

  // change transfer mode
  bool MODE(std::string const& cmd, std::stringstream& ss) {
    std::string mode;
    std::getline(ss, mode);

    // bad # args?
    if (mode.length() != 1) {
      respond_with_code(501);
      return false;
    }

    // only handle the Stream ("S") mode for now
    if (mode == "S") {
      // update session state
      this->current_mode = mode[0];

      respond_with("200 Switching to Stream mode.");
      return true;
    } else {
      respond_with_code(504);
      return false;
    }
  }

  // change transfer structure
  bool STRU(std::string const& cmd, std::stringstream& ss) {
    std::string stru;
    std::getline(ss, stru);

    // bad # args?
    if (stru.length() != 1) {
      respond_with_code(501);
      return false;
    }

    // only handle the File ("F") structure for now
    if (stru == "F") {
      // update session state
      this->current_structure = stru[0];

      respond_with("200 Switching to File structure.");
      return true;
    } else {
      respond_with_code(504);
      return false;
    }
  }

  // remove dir
  bool RMD(std::string const& cmd, std::stringstream& ss) {
    std::string path;
    std::getline(ss, path);

    // bad # args?
    if (path.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    if (this->_rmdir(path)) {
      respond_with_code(250);
      return true;
    } else {
      respond_with_code(550);
      return false;
    }
  }

  // make dir
  bool MKD(std::string const& cmd, std::stringstream& ss) {
    std::string path;
    std::getline(ss, path);

    // bad # args?
    if (path.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    if (this->_mkdir(path)) {
      std::string new_path;
      if (path[0] == '/') {
	new_path = this->_get_abspath(this->real_root+path);
      } else {
	new_path = this->_get_abspath(path);
      }
      respond_with(std::string("257 \"") + fakify(new_path) + "\" directory created");
      return true;
    } else {
      respond_with_code(550);
      return false;
    }
  }

  // set client data connection details
  bool PORT(std::string const& cmd, std::stringstream& ss) {
    std::string h1, h2, h3, h4, p1, p2;
    std::getline(ss, h1, ',');
    std::getline(ss, h2, ',');
    std::getline(ss, h3, ',');
    std::getline(ss, h4, ',');
    std::getline(ss, p1, ',');
    std::getline(ss, p2);

    // bad # args?
    if (h1.empty() || h2.empty() || h3.empty() || h4.empty() ||
	p1.empty() || p2.empty()) {
      respond_with_code(501);
      return false;
    }

    // check the input ip/port
    if (h1.back() == ' ') { h1.pop_back(); }
    if (h2.back() == ' ') { h2.pop_back(); }
    if (h3.back() == ' ') { h3.pop_back(); }
    if (h4.back() == ' ') { h4.pop_back(); }
    if (p1.back() == ' ') { p1.pop_back(); }
    if (p2.back() == ' ') { p2.pop_back(); }

    int port = atoi(p1.c_str()) * 256 + atoi(p2.c_str());

    if (h1.empty() || h2.empty() || h3.empty() || h4.empty() ||
	p1.empty() || p2.empty() || port == 0) {
      respond_with_code(501);
      return false;
    }

    // update session state
    this->data_ip = h1 + '.' + h2 + '.' + h3 + '.' + h4;
    this->data_port = port;

    this->_data_disconnect();
    respond_with_code(200);
    return true;
  }

  // helper method: establish a data connection
  bool _data_connect() {
    // already connected?
    if (this->data_connected) {
      return true;
    }

    // do we have enough info to connect?
    if (this->data_ip.empty() || this->data_port == 0) {
      return false;
    }

    // create socket and connect to dataip:dataport
    int dfd = socket(AF_INET, SOCK_STREAM, 0);

    if (dfd == -1) {
      return false;
    } else {
      this->data_fd = dfd;
    }

    bzero(&this->data_si, sizeof(this->data_si));
    this->data_si.sin_family = AF_INET;
    this->data_si.sin_addr.s_addr = inet_addr(this->data_ip.c_str());
    this->data_si.sin_port = htons(this->data_port);

    if (connect(this->data_fd, (sockaddr*)&this->data_si,
		sizeof(this->data_si)) == 0) {
      // update session state
      this->data_connected = true;
      return true;
    } else {
      return false;
    }
  }

  // helper method: close a data connection
  bool _data_disconnect() {
    // already disconnected?
    if (!this->data_connected) {
      return true;
    }

    if (this->data_fd == -1) {
      // update session state
      this->data_connected = false;
      return true;
    } else {
      int res = close(this->data_fd);
      // update session state
      this->data_fd = -1;
      this->data_connected = false;
      return res == 0;
    }
  }

  // list files via `ls` executable
  bool LIST(std::string const& cmd, std::stringstream& ss) {
    std::string opt;
    std::getline(ss, opt);

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    // require Image type for this operation
    if (this->current_type != 'I') {
      respond_with_code(451);
      return false;
    }

    // begin data connection
    respond_with_code(150);

    if (!this->_data_connect()) {
      respond_with_code(451);
      return false;
    }

    int pid = fork();

    if (pid == 0) {
      // child process: run `ls`
      dup2(this->data_fd, 1);

      if (!opt.empty() && opt[0] == '-') {
	execl("/bin/ls", "ls", opt.c_str(), NULL);
      }	else {
	execl("/bin/ls", "ls", NULL);
      }

      // dead code
      return true;
    } else {
      // parent process: wait for `ls` to complete
      ::wait(NULL);
    }

    // end data connection
    respond_with_code(226);
    this->_data_disconnect();
    return true;
  }

  // copy file to server via `cat` executable
  bool STOR(std::string const& cmd, std::stringstream& ss) {
    std::string filename;
    std::getline(ss, filename);

    // bad # args?
    if (filename.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    // require Image type for this operation
    if (this->current_type != 'I') {
      respond_with_code(451);
      return false;
    }

    // begin data connection
    respond_with_code(150);

    if (!this->_data_connect()) {
      respond_with_code(451);
      return false;
    }

    // the file we want to write on the server
    int fdout = creat(filename.c_str(), 0644);

    if (fdout == -1) {
      respond_with_code(450);
      return false;
    }

    int pid = fork();

    if (pid == 0) {
      // child process: run `cat` to copy data from client data socket -> server file
      dup2(this->data_fd, 0);
      dup2(fdout, 1);
      execl("/bin/cat", "cat", NULL);

      // dead code
      return true;
    } else {
      // parent process: wait for child to finish the STOR heavy-lifting
      ::wait(NULL);
    }

    // end data connection
    respond_with_code(226);
    this->_data_disconnect();
    return true;
  }

  // get file from server via `cat` executable
  bool RETR(std::string const& cmd, std::stringstream& ss) {
    std::string filename;
    std::getline(ss, filename);

    // bad # args?
    if (filename.empty()) {
      respond_with_code(501);
      return false;
    }

    // authorized?
    if (!this->logged_in) {
      respond_with_code(530);
      return false;
    }

    // require Image type for this operation
    if (this->current_type != 'I') {
      respond_with_code(451);
      return false;
    }

    // begin data connection
    respond_with_code(150);

    if (!this->_data_connect()) {
      respond_with_code(451);
      return false;
    }

    // the file we want to read from the server
    int fdin = open(filename.c_str(), O_RDONLY);

    if (fdin == -1) {
      respond_with_code(450);
      return false;
    }

    int pid = fork();

    if (pid == 0) {
      // child process: run `cat` to copy data from server file -> client data socket
      dup2(fdin, 0);
      dup2(this->data_fd, 1);
      execl("/bin/cat", "cat", NULL);

      // dead code
      return true;
    } else {
      // parent process: wait for child to finish the STOR heavy-lifting
      ::wait(NULL);
    }

    // end data connection
    respond_with_code(226);
    this->_data_disconnect();
    return true;
  }

  // wrapper method
  std::string _get_cwd() const {
    char tmp[PATH_MAX + 1] { };
    getcwd(tmp, PATH_MAX + 1);
    return tmp;
  }

  bool begins_with(std::string const& input, std::string const& match) const {
    return input.size() >= match.size() &&
      std::equal(match.begin(), match.end(), input.begin());
  }

  // "fakify" a path (long path -> short path)
  std::string fakify(std::string const& path) const {
    if (path.empty()) {
      return "/";
    }

    if (path == this->real_root) {
      return "/";
    }

    if (!this->begins_with(path, this->real_root)) {
      return "/";
    }

    return path.substr(this->real_root.length(), std::string::npos);
  }

  // wrapper method, with sandboxing
  bool _set_cwd(std::string const& path) {
    if (path.empty()) {
      return false;
    }

    std::string old_cwd(this->_get_cwd());

    if (chdir(path.c_str()) == 0) {
      std::string new_cwd(this->_get_cwd());

      // check if the new directory is outside our namespace:
      // does the new cwd start with the root dir? if not, go back
      if (this->begins_with(new_cwd, this->real_root)) {
	return true;
      } else {
	chdir(old_cwd.c_str());
	return false;
      }
    } else {
      return false;
    }
  }

  // wrapper method
  bool _rmdir(std::string const& path) const {
    if (path.empty()) {
      return false;
    }

    // handle absolute paths
    if (path[0] == '/') {
      std::string new_path = this->real_root + path;
      return rmdir(path.c_str()) == 0;
    } else {
      std::string abs_path = this->_get_abspath(path);

      // only delete things inside the namespace
      if (this->begins_with(abs_path, this->real_root)) {
	return rmdir(path.c_str()) == 0;
      } else {
	return false;
      }
    }
  }

  // wrapper method
  bool _mkdir(std::string const& path) const {
    if (path.empty()) {
      return false;
    }

    // handle absolute paths
    if (path[0] == '/') {
      std::string new_path = this->real_root + path;
      return mkdir(new_path.c_str(), S_IRWXU) == 0;
    } else {
      std::string abs_path = this->_get_abspath(path);

      // only create things inside the namespace
      if (this->begins_with(abs_path, this->real_root)) {
	return mkdir(path.c_str(), S_IRWXU) == 0;
      } else {
	return false;
      }
    }
  }

  // wrapper method
  std::string _get_abspath(std::string const& path) const {
    if (path.empty()) {
      return std::string();
    }

    char tmp[PATH_MAX + 1] { };
    realpath(path.c_str(), tmp);
    return tmp;
  }

  // the only constructor
  explicit Session(int fd_, sockaddr_in& sender_) :
    fd(fd_), sender(sender_), running(true), current_type('A'),
    current_mode('S'), current_structure('F'), logged_in(false),
    data_connected(false), data_port(0), data_fd(-1),
    real_root(_get_abspath(".")) {
  }

  // clean up resources
  virtual ~Session() {
    if (this->fd != -1) {
      close(this->fd);
    }

    this->_data_disconnect();
    this->_set_cwd(this->real_root);
  }

  int fd;
  sockaddr_in sender;

  bool running;

  char current_type;
  char current_mode;
  char current_structure;
  bool logged_in;
  std::string current_user;

  bool data_connected;
  std::string data_ip;
  int data_port;
  int data_fd;
  sockaddr_in data_si;

  std::string real_root;
};

#endif
