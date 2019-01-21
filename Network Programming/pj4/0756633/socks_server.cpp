#include <sys/wait.h>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address_v4.hpp>
#include <boost/asio/ip/network_v4.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include "socks4_namespace.hpp"

using namespace std;
using boost::asio::ip::tcp;
boost::asio::io_service io_service;

class server {
 public:
  server(unsigned short port)
      : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
    start_signal_wait();
    start_accept();
  }

 private:
  void start_signal_wait() {
    signal_.async_wait(boost::bind(&server::handle_signal_wait, this));
  }

  void start_accept() {
    acceptor_.async_accept(source_socket_,
                           boost::bind(&server::handle_accept, this, _1));
  }

  void handle_signal_wait() {
    if (acceptor_.is_open()) {
      int status = 0;
      while (waitpid(-1, &status, WNOHANG) > 0) {
      }

      start_signal_wait();
    }
  }

  void handle_accept(const boost::system::error_code& ec) {
    if (!ec) {
      io_service.notify_fork(boost::asio::io_context::fork_prepare);
      if (fork() == 0) {
        io_service.notify_fork(boost::asio::io_context::fork_child);
        acceptor_.close();
        signal_.cancel();

        try {
          receive_socks4_package();
        } catch (std::system_error& e) {
          destination_socket_.close();
          source_socket_.close();
        }

      } else {
        io_service.notify_fork(boost::asio::io_context::fork_parent);
        source_socket_.close();
        destination_socket_.close();

        start_accept();
      }
    } else {
      cerr << "Accept error: " << ec.message() << endl;
      start_accept();
    }
  }

  void receive_socks4_package() {
    boost::asio::read(source_socket_, socks_req_.buffers());

    string D_IP, D_PORT, Command;
    tie(D_IP, D_PORT, Command) = socks_req_.get_socks_status();

    if (!can_pass_firewall(D_IP)) {
      cout << get_format() % D_IP % D_PORT % Command %
                  source_socket_.remote_endpoint().address().to_string() %
                  source_socket_.remote_endpoint().port() % "Reject";
      socks4::reply_builder reply(socks4::reply_builder::Reject, socks_req_);
      boost::asio::write(source_socket_, reply.buffers());
      return;
    }

    cout << get_format() % D_IP % D_PORT % Command %
                source_socket_.remote_endpoint().address().to_string() %
                source_socket_.remote_endpoint().port() % "Accept";

    if (socks_req_.command_ ==
        socks4::request_interpreter::command_type::connect)
      resolve(tcp::resolver::query(socks_req_.getAddress(),
                                   to_string(socks_req_.getPort())));

    else if (socks_req_.command_ ==
             socks4::request_interpreter::command_type::bind) {
      tcp::acceptor acceptor_for_bind_{io_service};
      tcp::endpoint endpoint(tcp::v4(), 0);

      acceptor_for_bind_.open(endpoint.protocol());
      acceptor_for_bind_.bind(endpoint);
      acceptor_for_bind_.listen();
      acceptor_for_bind_.local_endpoint().port();

      socks_req_.port_high_byte_ =
          acceptor_for_bind_.local_endpoint().port() >> 8;
      socks_req_.port_low_byte_ = acceptor_for_bind_.local_endpoint().port();

      socks_req_.address_ =
          boost::asio::ip::make_address_v4("0.0.0.0").to_bytes();

      socks4::reply_builder reply(socks4::reply_builder::Accept, socks_req_);
      boost::asio::write(source_socket_, reply.buffers());

      acceptor_for_bind_.accept(destination_socket_);

      socks4::reply_builder reply2(socks4::reply_builder::Accept, socks_req_);
      boost::asio::write(source_socket_, reply2.buffers());

      async_read_from_destination();
      async_read_from_source();
    }
  }

  void resolve(const tcp::resolver::query& q) {
    resolver_.async_resolve(q, [this](const boost::system::error_code& ec,
                                      tcp::resolver::iterator it) {
      if (!ec) {
        connect(it);
      }
    });
  }

  void connect(const tcp::resolver::iterator& it) {
    destination_socket_.async_connect(
        *it, [this](const boost::system::error_code& ec) {
          if (!ec) {
            socks4::reply_builder reply(socks4::reply_builder::Accept,
                                        socks_req_);
            boost::asio::write(source_socket_, reply.buffers());

            async_read_from_destination();
            async_read_from_source();
          }
        });
  }

  boost::format get_format() {
    return boost::format(
        "<S_IP>:%4%\n"
        "<S_PORT>:%5%\n"
        "<D_IP>:%1%\n"
        "<D_PORT>:%2%\n"
        "<Command>:%3%\n"
        "<Reply>:%6%\n\n");
  }

  bool can_pass_firewall(string D_IP) {
    ifstream firewall_stream("./socks.conf");
    string line;
    vector<string> white_list;
    string criteria = "permit c ";

    if (socks_req_.command_ ==
        socks4::request_interpreter::command_type::connect) {
      criteria = "permit c ";
    } else if (socks_req_.command_ ==
               socks4::request_interpreter::command_type::bind) {
      criteria = "permit b ";
    }

    while (getline(firewall_stream, line)) {
      if (line.substr(0, criteria.size()) == criteria) {
        white_list.push_back(line.substr(criteria.size()));
      }
    }

    for (const string& ip : white_list) {
      auto prefix = ip.substr(0, ip.find('*'));
      if (D_IP.substr(0, prefix.size()) == prefix) {
        return true;
      }
    }

    return false;
  }

  void async_read_from_destination() {
    destination_socket_.async_receive(
        boost::asio::buffer(destination_buffer_),
        [this](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            async_write_to_source(length);
          } else {
            throw system_error{ec};
          }
        });
  }

  void async_write_to_source(size_t length) {
    boost::asio::async_write(
        source_socket_, boost::asio::buffer(destination_buffer_, length),
        [this](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            async_read_from_destination();
          } else {
            throw system_error{ec};
          }
        });
  }

  void async_read_from_source() {
    source_socket_.async_receive(
        boost::asio::buffer(source_buffer_),
        [this](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            async_write_to_destination(length);
          } else {
            throw system_error{ec};
          }
        });
  }

  void async_write_to_destination(size_t length) {
    boost::asio::async_write(
        destination_socket_, boost::asio::buffer(source_buffer_, length),
        [this](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            async_read_from_source();
          } else {
            throw system_error{ec};
          }
        });
  }

  boost::asio::signal_set signal_{io_service};
  tcp::resolver resolver_{io_service};
  tcp::acceptor acceptor_{io_service};

  tcp::socket source_socket_{io_service};  // This socket connects to a
                                           // browser (e.g. firefox),
                                           // it is going to communicate
                                           // with a client.

  tcp::socket destination_socket_{io_service};  // This socket connects
                                                // to a website, so it is
                                                // going to communicate
                                                // with the website.
  array<unsigned char, 65536> source_buffer_{};
  array<unsigned char, 65536> destination_buffer_{};

  socks4::request_interpreter socks_req_;
};

int main(int argc, char* argv[]) {
  if (argc != 2) {
    cerr << "Usage: ./socks_server <port>\n";
    return 1;
  }

  try {
    server s(atoi(argv[1]));
    io_service.run();
  } catch (std::exception& ec) {
  }
}