#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <map>
#include <sys/wait.h>
#include <unistd.h>
#include "string_manipulator.h"

using boost::asio::ip::tcp;
using namespace std;

struct Parser final {
  static Parser &getInstance() {
    static Parser instance;
    return instance;
  }

  Parser(const Parser &) = delete;

  void operator=(const Parser &) = delete;

  static vector<string> split_and_trim(string cmd,
                                       const string &predicate = "\t\n ") {
    vector<string> ret;
    cmd = boost::trim_copy(cmd);
    boost::split(ret, cmd, boost::is_any_of(predicate),
                 boost::token_compress_on);
    for (auto &item : ret) boost::trim(item);
    return ret;
  }

  map<string, string> parse_http_request(string request) const {
    vector<string> http_request;

    for (auto lhs = request.begin(), rhs = request.begin();
         rhs!=request.end(); ++rhs) {
      if (*rhs=='\n') {
        string line(lhs, rhs);
        if (line.back()=='\r') line.pop_back();
        http_request.push_back(line);
        lhs = rhs;
      }
    }

    map<string, string> parser;

    for (auto line : http_request) {
      auto key = line.substr(0, line.find(':'));
      auto value = line.substr(key.size());
      if (key!=line) {
        value = value.substr(2);
        if (key.front()=='\n') key = key.substr(1);
        parser.insert({key, value});
      } else {
        trim_if(line, boost::is_any_of("\n\r"));
        if (line.empty()) continue;

        auto header = split_and_trim(line);

        auto METHOD = header.at(0);
        parser.insert({"REQUEST_METHOD", METHOD});

        auto URI = header.at(1);
        parser.insert({"REQUEST_URI", URI});

        parser.insert({"SERVER_PROTOCOL", header.at(2)});

        auto target = URI.substr(0, URI.find('?'));
        parser.insert({"TARGET", target});

        if (URI!=target) {
          auto query = URI.substr(target.size() + 1);
          parser.insert({"QUERY_STRING", query});
        } else {
          parser.insert({"QUERY_STRING", ""});
        }
      }
    }

    try {
      string name = parser.at("Host");
      parser.insert({"HTTP_HOST", name});
    } catch (exception &e) {
      cerr << e.what() << endl;
    }

    return parser;
  }

 private:
  Parser() = default;
};

class http_server {
 public:
  explicit http_server(boost::asio::io_service &io_service, uint16_t port)
      : _io_service(io_service),
        _signal(io_service, SIGCHLD),
        _acceptor(io_service, {tcp::v4(), port}),
        _socket(io_service) {
    wait_for_signal();
    do_accept();
  }

 private:
  void wait_for_signal() {
    _signal.async_wait(
        [this](boost::system::error_code /*ec*/, int /*signo*/) {
          // Only the parent process should check for this signal. We can
          // determine whether we are in the parent by checking if the acceptor
          // is still open.
          if (_acceptor.is_open()) {
            // Reap completed child processes so that we don't end up with
            // zombies.
            int status = 0;
            while (waitpid(-1, &status, WNOHANG) > 0) {}
            wait_for_signal();
          }
        });
  }

  void do_accept() {
    _acceptor.async_accept(
        [this](boost::system::error_code ec, tcp::socket new_socket) {
          if (!ec) {
            // Take ownership of the newly accepted socket.
            _socket = std::move(new_socket);

            // Inform the io_service that we are about to fork. The io_service
            // cleans up any internal resources, such as threads, that may
            // interfere with forking.
            _io_service.notify_fork(boost::asio::io_service::fork_prepare);

            if (fork()==0) {
              // Inform the io_service that the fork is finished and that this
              // is the child process. The io_service uses this opportunity to
              // create any internal file descriptors that must be private to
              // the new process.
              _io_service.notify_fork(boost::asio::io_service::fork_child);

              // The child won't be accepting new connections, so we can close
              // the acceptor. It remains open in the parent.
              _acceptor.close();

              // The child process is not interested in processing the SIGCHLD
              // signal.
              _signal.cancel();

              do_read();

            } else {
              // Inform the io_service that the fork is finished (or failed)
              // and that this is the parent process. The io_service uses this
              // opportunity to recreate any internal resources that were
              // cleaned up during preparation for the fork.
              _io_service.notify_fork(boost::asio::io_service::fork_parent);

              // The parent process can now close the newly accepted socket. It
              // remains open in the child.
              _socket.close();
              do_accept();
            }
          } else {
            std::cerr << "Accept error: " << ec.message() << std::endl;
            do_accept();

          }
        });
  }

  void do_read() {
    _socket.async_read_some(boost::asio::buffer(_data),
                            [this](boost::system::error_code ec,
                                   std::size_t length) {
                              if (!ec) {
                                auto result = Parser::getInstance().parse_http_request(
                                    string(_data.begin(), _data.begin() + length));

                                setenv("REQUEST_METHOD",
                                       result["REQUEST_METHOD"].c_str(), 1);

                                setenv("REQUEST_URI", result["REQUEST_URI"].c_str(), 1);

                                setenv("QUERY_STRING", result["QUERY_STRING"].c_str(), 1);

                                setenv("SERVER_PROTOCOL", result["SERVER_PROTOCOL"].c_str(), 1);

                                setenv("HTTP_HOST", result["HTTP_HOST"].c_str(), 1);

                                setenv("SERVER_ADDR",
                                       _socket.local_endpoint().address().to_string().c_str(), 1);
                                setenv("SERVER_PORT",
                                       to_string(_socket.local_endpoint().port()).c_str(), 1);
                                setenv("REMOTE_ADDR",
                                       _socket.remote_endpoint().address().to_string().c_str(), 1);
                                setenv("REMOTE_PORT",
                                       to_string(_socket.remote_endpoint().port()).c_str(), 1);

                                // ------------------------------------- dup -------------------------------------
                                dup2(_socket.native_handle(), 0);
                                dup2(_socket.native_handle(), 1);
                                dup2(_socket.native_handle(), 2);

                                // ------------------------------------- exec -------------------------------------
                                std::cout << "HTTP/1.1" << " 200 OK\r\n";
                                std::cout.flush();
                                auto target = result["TARGET"];
                                target = target.substr(1);
                                char *argv[] = {nullptr};
                                if (execv(target.c_str(), argv)==-1) {
                                  perror("execv error");
                                  exit(-1);
                                }
                              }
                            });
  }

  boost::asio::io_service &_io_service;
  boost::asio::signal_set _signal;
  tcp::acceptor _acceptor;
  tcp::socket _socket;
  std::array<char, 1024> _data;
};

int main(int argc, char *argv[]) {
  signal(SIGCHLD, SIG_IGN);
  using namespace std;
  try {
    if (argc!=2) {
      std::cerr << "Usage: process_per_connection <port>\n";
      return 1;
    }
    boost::asio::io_service io_service;
    http_server s(io_service, stoi(string(argv[1])));
    io_service.run();
  }
  catch (exception &e) {
    cerr << "Exception: " << e.what() << endl;
  }
}
