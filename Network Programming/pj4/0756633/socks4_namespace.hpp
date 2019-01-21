#pragma once

#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/array.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/format.hpp>
#include <iostream>
#include <ostream>
#include <string>

namespace socks4 {

using std::fstream;
using std::ostream;
using std::string;
using std::to_string;
using std::tuple;

const unsigned char version = 0x04;

class request_interpreter {
 public:
  enum command_type { connect = 0x01, bind = 0x02 };

  std::array<boost::asio::mutable_buffer, 7> buffers() {
    return {
        {boost::asio::buffer(&version_, 1), boost::asio::buffer(&command_, 1),
         boost::asio::buffer(&port_high_byte_, 1),
         boost::asio::buffer(&port_low_byte_, 1), boost::asio::buffer(address_),
         boost::asio::buffer(user_id_), boost::asio::buffer(&null_byte_, 1)}};
  }

  tuple<string, string, string> get_socks_status() {
    return make_tuple(getAddress(), to_string(getPort()), getCommand());
  }

  unsigned int getPort() {
    unsigned short port = port_high_byte_;
    port = (port << 8) & 0xff00;
    port = port | port_low_byte_;
    return port;
  }

  string getAddress() {
    return boost::asio::ip::address_v4(address_).to_string();
  }

  string getCommand() {
    std::string Command;
    if (command_ == connect) {
      return "Connect";
    } else if (command_ == bind) {
      return "Bind";
    }
    return "Unknown";
  }

  friend ostream& operator<<(ostream& os, const request_interpreter& req) {
    boost::format fmt(
        "--- INTERPRET REQUEST ---\n"
        "version_        = %1%\n"
        "command_        = %2%\n"
        "port            = %3%\n"
        "address_        = %4%\n"
        "uid             = %5%\n");

    os << fmt % req.version_ % (int)req.command_ %
              ((req.port_high_byte_ * 256) + req.port_low_byte_) %
              boost::asio::ip::address_v4(req.address_).to_string() %
              req.user_id_.size();

    return os;
  }

  unsigned char version_;
  unsigned char command_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_;
  string user_id_;
  unsigned char null_byte_;
};

class reply_builder {
 public:
  enum status_type {
    Accept = 0x5a,
    Reject = 0x5b,
  };

  reply_builder(status_type status, request_interpreter req)
      : null_byte_(0),
        status_(status),
        port_high_byte_(req.port_high_byte_),
        port_low_byte_(req.port_low_byte_),
        address_(req.address_) {}

  std::array<boost::asio::const_buffer, 5> buffers() const {
    return {{boost::asio::buffer(&null_byte_, 1),
             boost::asio::buffer(&status_, 1),
             boost::asio::buffer(&port_high_byte_, 1),
             boost::asio::buffer(&port_low_byte_, 1),
             boost::asio::buffer(address_)}};
  }

  friend ostream& operator<<(ostream& os, const reply_builder& builder) {
    boost::format fmt(
        "------ REPLY ------\n"
        "null_byte_      = %1%\n"
        "status_         = %2%\n"
        "port            = %3%\n"
        "address_        = %4%\n");
    os << fmt % builder.null_byte_ % builder.status_ %
              ((builder.port_high_byte_ * 256) + builder.port_low_byte_) %
              boost::asio::ip::address_v4(builder.address_).to_string();

    return os;
  }

 private:
  unsigned char null_byte_;
  unsigned char status_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_{};
};

class request {
 public:
  enum command_type { connect = 0x01, bind = 0x02 };

  request(command_type cmd, const boost::asio::ip::tcp::endpoint& endpoint,
          string uid)
      : version_(version), command_(cmd) {
    // Only IPv4 is supported by the SOCKS 4 protocol.
    if (endpoint.protocol() != boost::asio::ip::tcp::v4()) {
      throw boost::system::system_error(
          boost::asio::error::address_family_not_supported);
    }

    // Convert port number to network byte order.
    unsigned short port = endpoint.port();
    port_high_byte_ = (port >> 8) & 0xff;
    port_low_byte_ = port & 0xff;

    // Save IP address in network byte order.
    address_ = endpoint.address().to_v4().to_bytes();
    user_id_ = uid;
  }

  std::array<boost::asio::const_buffer, 7> buffers() const {
    return {
        {boost::asio::buffer(&version_, 1), boost::asio::buffer(&command_, 1),
         boost::asio::buffer(&port_high_byte_, 1),
         boost::asio::buffer(&port_low_byte_, 1), boost::asio::buffer(address_),
         boost::asio::buffer(user_id_), boost::asio::buffer(&null_byte_, 1)}};
  }

  friend ostream& operator<<(ostream& os, const request& req) {
    boost::format fmt(
        "------ REQUEST ------\n"
        "version_        = %1%\n"
        "command_        = %2%\n"
        "port            = %3%\n"
        "address_        = %4%\n"
        "uid             = %5%\n");

    os << fmt % req.version_ % (int)req.command_ %
              ((req.port_high_byte_ * 256) + req.port_low_byte_) %
              boost::asio::ip::address_v4(req.address_).to_string() %
              req.user_id_;

    return os;
  }

  unsigned char version_;
  unsigned char command_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_;
  string user_id_;
  unsigned char null_byte_;
};

class reply {
 public:
  enum status_type {
    request_granted = 0x5a,
    request_failed = 0x5b,
    request_failed_no_identd = 0x5c,
    request_failed_bad_user_id = 0x5d
  };

  reply() : null_byte_(0), status_() {}

  std::array<boost::asio::mutable_buffer, 5> buffers() {
    return {{boost::asio::buffer(&null_byte_, 1),
             boost::asio::buffer(&status_, 1),
             boost::asio::buffer(&port_high_byte_, 1),
             boost::asio::buffer(&port_low_byte_, 1),
             boost::asio::buffer(address_)}};
  }

  bool success() const { return null_byte_ == 0 && status_ == request_granted; }

  unsigned char status() const { return status_; }

  boost::asio::ip::tcp::endpoint endpoint() const {
    unsigned short port = port_high_byte_;
    port = (port << 8) & 0xff00;
    port = port | port_low_byte_;

    boost::asio::ip::address_v4 address(address_);

    return boost::asio::ip::tcp::endpoint(address, port);
  }

  friend ostream& operator<<(ostream& os, const reply& reply) {
    boost::format fmt(
        "------ REPLY ------\n"
        "null_byte_      = %1%\n"
        "status_         = %2%\n"
        "port            = %3%\n"
        "address_        = %4%\n");
    os << fmt % reply.null_byte_ % reply.status_ %
              ((reply.port_high_byte_ * 256) + reply.port_low_byte_) %
              boost::asio::ip::address_v4(reply.address_).to_string();

    return os;
  }

 private:
  unsigned char null_byte_;
  unsigned char status_;
  unsigned char port_high_byte_;
  unsigned char port_low_byte_;
  boost::asio::ip::address_v4::bytes_type address_;
};

}  // namespace socks4
