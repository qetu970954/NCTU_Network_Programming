#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "socks4_namespace.hpp"

using namespace boost::asio;
using namespace boost::asio::ip;

io_service service;

class fancy_console {
  struct Host {
    std::string name;
    std::string port;
    std::string file;

    bool exist() const {
      return !(name.empty() && port.empty() && file.empty());
    }
  };

 public:
  static fancy_console& get_instance() {
    static fancy_console instance;
    return instance;
  }

  fancy_console(const fancy_console&) = delete;

  void operator=(const fancy_console&) = delete;

  std::string fancy_string() {
    std::string fancy_console_string =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <meta charset=\"UTF-8\" />\n"
        "    <title>NP Project 3 Console</title>\n"
        "    <link\n"
        "      rel=\"stylesheet\"\n"
        "      "
        "href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/"
        "bootstrap.min.css\"\n"
        "      "
        "integrity=\"sha384-MCw98/"
        "SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
        "      crossorigin=\"anonymous\"\n"
        "    />\n"
        "    <link\n"
        "      "
        "href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "      rel=\"stylesheet\"\n"
        "    />\n"
        "    <link\n"
        "      rel=\"icon\"\n"
        "      type=\"image/png\"\n"
        "      "
        "href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/"
        "678068-terminal-512.png\"\n"
        "    />\n"
        "    <style>\n"
        "      * {\n"
        "        font-family: 'Source Code Pro', monospace;\n"
        "        font-size: 1rem !important;\n"
        "      }\n"
        "      body {\n"
        "        background-color: #212529;\n"
        "      }\n"
        "      pre {\n"
        "        color: #cccccc;\n"
        "      }\n"
        "      b {\n"
        "        color: #ffffff;\n"
        "      }\n"
        "    </style>\n"
        "  </head>\n";

    boost::format fmt(
        "  <body>\n"
        "    <table class=\"table table-dark table-bordered\">\n"
        "      <thead>\n"
        "        <tr>\n"
        "%1%"
        "        </tr>\n"
        "      </thead>\n"
        "      <tbody>\n"
        "        <tr>\n"
        "%2%"
        "        </tr>\n"
        "      </tbody>\n"
        "    </table>\n"
        "  </body>\n");

    std::string table;
    std::string session;
    for (size_t i = 0; i < hosts.size(); ++i) {
      Host& h = hosts.at(i);
      if (h.exist()) {
        table +=
            "          <th scope=\"col\">" + h.name + ":" + h.port + "</th>\n";
        session += "          <td><pre id=\"s" + std::to_string(i) +
                   "\" class=\"mb-0\"></pre></td>";
      }
    }

    return fancy_console_string + (fmt % table % session).str() + "</html>\n";
  }

  std::array<Host, 5> hosts;

 private:
  fancy_console() = default;
};

class appender {
 public:
  static appender& get_instance() {
    static appender instance;
    return instance;
  }

  appender(const appender&) = delete;

  void operator=(const appender&) = delete;

  void output_shell(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt(
        "<script>document.getElementById('%1%').innerHTML += '%2%';</script>");
    std::cout << fmt % session % content;
    std::cout.flush();
  }

  void output_command(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt(
        "<script>document.getElementById('%1%').innerHTML += "
        "'<b>%2%</b>';</script>");
    std::cout << fmt % session % content;
    std::cout.flush();
  }

 private:
  appender() = default;

  void encode(std::string& data) {
    using boost::algorithm::replace_all;
    replace_all(data, "&", "&amp;");
    replace_all(data, "\"", "&quot;");
    replace_all(data, "\'", "&apos;");
    replace_all(data, "<", "&lt;");
    replace_all(data, ">", "&gt;");
  }
};

struct client : public std::enable_shared_from_this<client> {
  client(std::string session, std::string file_name, tcp::resolver::query q)
      : session(std::move(session)), q(std::move(q)) {
    file.open("test_case/" + file_name, std::ios::in);
    if (!file.is_open())
      appender::get_instance().output_command(session, "DAMN file not open!");
  }

  void read_handler() {
    auto self(shared_from_this());
    tcp_socket.async_receive(
        buffer(bytes),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            std::string data(
                bytes.begin(),
                bytes.begin() + length);  // this might not be a entire line!
            appender::get_instance().output_shell(session, data);

            if (data.find("% ") != std::string::npos) {
              std::string command;
              getline(file, command);
              appender::get_instance().output_command(session, command + '\n');
              tcp_socket.write_some(buffer(command + '\n'));
              // boost::asio::write(tcp_socket, buffer(command + "\r\n",
              // command.size()+2));
            }
            read_handler();
          }
        });
  }

  void connect_handler() { read_handler(); }

  void resolve_handler(tcp::resolver::iterator it) {
    auto self(shared_from_this());
    tcp_socket.async_connect(*it, [this, self](boost::system::error_code ec) {
      if (!ec) connect_handler();
    });
  }

  void resolve() {
    auto self(shared_from_this());
    resolver.async_resolve(q, [this, self](const boost::system::error_code& ec,
                                           tcp::resolver::iterator it) {
      if (!ec) resolve_handler(it);
    });
  }

  std::string session;  // e.g. "s0"
  tcp::resolver::query q;
  tcp::resolver resolver{service};
  tcp::socket tcp_socket{service};
  std::array<char, 4096> bytes;
  std::fstream file;  // e.g. "t1.txt"
};

struct client_with_socks
    : public std::enable_shared_from_this<client_with_socks> {
  client_with_socks(std::string session, std::string file_name,
                    tcp::resolver::query q, std::string http_server,
                    std::string http_port)
      : session_(std::move(session)),
        q_(std::move(q)),
        http_server_(http_server),
        http_port_(http_port) {
    test_case_.open("test_case/" + file_name, std::ios::in);
    if (!test_case_.is_open())
      appender::get_instance().output_command(session, "DAMN file not open!");
  }

  void read_handler() {
    auto self(shared_from_this());
    socket_.async_receive(
        buffer(bytes_),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            std::string data(
                bytes_.begin(),
                bytes_.begin() + length);  // this might not be a entire line!
            appender::get_instance().output_shell(session_, data);

            if (data.find("% ") != std::string::npos) {
              std::string command;
              getline(test_case_, command);
              appender::get_instance().output_command(session_, command + '\n');
              boost::asio::write(socket_, buffer(command + '\n'));
            }
            read_handler();
          }
        });
  }

  void resolve() {
    tcp::resolver resolver(service);

    auto endpoints = resolver.resolve(q_);

    boost::asio::connect(socket_, endpoints);

    auto http_endpoint =
        *resolver.resolve(tcp::resolver::query(http_server_, http_port_));

    socks4::request socks_request(socks4::request::connect, http_endpoint, "");
    boost::asio::write(socket_, socks_request.buffers(),
                       boost::asio::transfer_all());

    socks4::reply socks_reply;
    boost::asio::read(socket_, socks_reply.buffers(),
                      boost::asio::transfer_all());

    if (!socks_reply.success()) {
      std::cout << "Connection failed.\n";
      std::cout << "status = 0x" << std::hex << socks_reply.status();
      return;
    }

    read_handler();
  }

  std::string session_;  // e.g. "s0"
  tcp::resolver::query q_;
  tcp::socket socket_{service};
  std::array<char, 4096> bytes_;
  std::fstream test_case_;  // e.g. "t1.txt"
  std::string http_server_;
  std::string http_port_;
};

std::string envVariableQuery(std::string s) {
  char* result = getenv(s.c_str());
  if (result == nullptr)
    return "None";
  else
    return result;
}

std::vector<std::string> splitAndTrim(const std::string& command,
                                      const std::string& predicate = "\t\n ") {
  std::string command_cpy(command);
  boost::trim(command_cpy);
  std::vector<std::string> ret;
  boost::split(ret, command_cpy, boost::is_any_of(predicate),
               boost::token_compress_on);
  for (auto& item : ret) boost::trim(item);
  return ret;
}

int main() {

  std::string socks_server;
  std::string socks_port;

  // ------------------------------------- PARSE QUERY
  // -------------------------------------
  for (const std::string& s :
       splitAndTrim(envVariableQuery("QUERY_STRING"), "&")) {
    std::string key = s.substr(0, s.find('='));
    std::string value = s.substr(key.size() + 1);

    if (key == "sh") {
      socks_server = value;
      continue;
    } else if (key == "sp") {
      socks_port = value;
      continue;
    }

    if (!value.empty()) {
      char attribute = key.front();
      int pos = key.back() - '0';
      if (attribute == 'h')
        fancy_console::get_instance().hosts.at(pos).name = value;
      else if (attribute == 'p')
        fancy_console::get_instance().hosts.at(pos).port = value;
      else if (attribute == 'f')
        fancy_console::get_instance().hosts.at(pos).file = value;
    }
  }

  std::cout << "Content-type: text/html\r\n\r\n";

  std::cout << fancy_console::get_instance().fancy_string();

  const auto& hosts = fancy_console::get_instance().hosts;
  for (size_t i = 0; i < hosts.size(); ++i) {
    auto& h = hosts.at(i);
    if (h.exist()) {
      auto sess = "s" + std::to_string(i);
      if (!socks_server.empty() && !socks_port.empty()) {
        tcp::resolver::query q{socks_server, socks_port};
        std::make_shared<client_with_socks>(sess, h.file, std::move(q), h.name,
                                            h.port)
            ->resolve();
      } else {
        tcp::resolver::query q{h.name, h.port};
        std::make_shared<client>(sess, h.file, std::move(q))->resolve();
      }
    }
  }

  service.run();
}