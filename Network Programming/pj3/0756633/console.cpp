#include <array>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <string>
#include "string_manipulator.h"

using namespace boost::asio;
using namespace boost::asio::ip;

io_service service;

class FancyConsole {
 public:
  static FancyConsole &getInstance() {
    static FancyConsole instance;
    return instance;
  }

  FancyConsole(const FancyConsole &) = delete;

  void operator=(const FancyConsole &) = delete;

  struct Host {
    std::string name;
    std::string port;
    std::string file;

    bool exist() const {
      return !(name.empty() && port.empty() && file.empty());
    }
  };

  std::array<Host, 5> hosts;

  std::string fancy_string() {
    std::string fancy_console_string =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <meta charset=\"UTF-8\" />\n"
        "    <title>NP Project 3 Console</title>\n"
        "    <link\n"
        "      rel=\"stylesheet\"\n"
        "      href=\"https://stackpath.bootstrapcdn.com/bootstrap/4.1.3/css/bootstrap.min.css\"\n"
        "      integrity=\"sha384-MCw98/SFnGE8fJT3GXwEOngsV7Zt27NXFoaoApmYm81iuXoPkFOJwJ8ERdknLPMO\"\n"
        "      crossorigin=\"anonymous\"\n"
        "    />\n"
        "    <link\n"
        "      href=\"https://fonts.googleapis.com/css?family=Source+Code+Pro\"\n"
        "      rel=\"stylesheet\"\n"
        "    />\n"
        "    <link\n"
        "      rel=\"icon\"\n"
        "      type=\"image/png\"\n"
        "      href=\"https://cdn0.iconfinder.com/data/icons/small-n-flat/24/678068-terminal-512.png\"\n"
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

    boost::format fmt("  <body>\n"
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
      Host &h = hosts.at(i);
      if (h.exist()) {
        table +=
            "          <th scope=\"col\">" + h.name + ":" + h.port + "</th>\n";
        session += "          <td><pre id=\"s" + std::to_string(i)
            + "\" class=\"mb-0\"></pre></td>";
      }
    }

    return fancy_console_string + (fmt%table%session).str() + "</html>\n";
  }

 private:
  FancyConsole() = default;
};

class Appender {
 public:
  static Appender &getInstance() {
    static Appender instance;
    return instance;
  }

  Appender(const Appender &) = delete;

  void operator=(const Appender &) = delete;

  void output_shell(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt
        ("<script>document.getElementById('%1%').innerHTML += '%2%';</script>");
    std::cout << fmt%session%content;
    std::cout.flush();
  }

  void output_command(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt
        ("<script>document.getElementById('%1%').innerHTML += '<b>%2%</b>';</script>");
    std::cout << fmt%session%content;
    std::cout.flush();
  }

 private:
  Appender() = default;

  void encode(std::string &data) {
    using boost::algorithm::replace_all;
    replace_all(data, "&", "&amp;");
    replace_all(data, "\"", "&quot;");
    replace_all(data, "\'", "&apos;");
    replace_all(data, "<", "&lt;");
    replace_all(data, ">", "&gt;");
  }
};

struct Client : public std::enable_shared_from_this<Client> {
  Client(std::string session, std::string file_name, tcp::resolver::query q)
      : session(std::move(session)), q(std::move(q)) {
    file.open("test_case/" + file_name, std::ios::in);
    if (!file.is_open())
      Appender::getInstance().output_command(session, "DAMN file not open!");
  }

  void read_handler() {
    auto self(shared_from_this());
    tcp_socket.async_receive(buffer(bytes),
                             [this, self](boost::system::error_code ec,
                                          std::size_t length) {
                               if (!ec) {
                                 std::string data(bytes.begin(),
                                                  bytes.begin()
                                                      + length); // this might not be a entire line!
                                 Appender::getInstance()
                                     .output_shell(session, data);

                                 if (data.find("% ")!=std::string::npos) {
                                   std::string command;
                                   getline(file, command);
                                   Appender::getInstance()
                                       .output_command(session, command + '\n');
                                   tcp_socket
                                       .write_some(buffer(command + '\n'));
                                   //boost::asio::write(tcp_socket, buffer(command + "\r\n", command.size()+2));
                                 }
                                 read_handler();
                               }
                             });
  }

  void connect_handler() {
    read_handler();
  }

  void resolve_handler(tcp::resolver::iterator it) {
    auto self(shared_from_this());
    tcp_socket.async_connect(*it, [this, self](boost::system::error_code ec) {
      if (!ec)
        connect_handler();
    });
  }

  void resolve() {
    auto self(shared_from_this());
    resolver.async_resolve(q,
                           [this, self](const boost::system::error_code &ec,
                                        tcp::resolver::iterator it) {
                             if (!ec)
                               resolve_handler(it);
                           });
  }

  std::string session; // e.g. "s0"
  tcp::resolver::query q;
  tcp::resolver resolver{service};
  tcp::socket tcp_socket{service};
  std::array<char, 4096> bytes;
  std::fstream file; // e.g. "t1.txt"
};

std::string envVariableQuery(std::string s) {
  char *result = getenv(s.c_str());
  if (result==nullptr)
    return "None";
  else
    return result;
}

std::vector<std::string> splitAndTrim(const std::string &command,
                                      const std::string &predicate = "\t\n ") {
  std::string command_cpy(command);
  boost::trim(command_cpy);
  std::vector<std::string> ret;
  boost::split(ret,
               command_cpy,
               boost::is_any_of(predicate),
               boost::token_compress_on);
  for (auto &item : ret)
    boost::trim(item);
  return ret;
}

int main() {

  // ------------------------------------- PARSE QUERY -------------------------------------
  for (const std::string &s : splitAndTrim(envVariableQuery("QUERY_STRING"),
                                           "&")) {
    std::string key = s.substr(0, s.find('='));
    std::string value = s.substr(key.size() + 1);
    if (!value.empty()) {
      char attribute = key.front();
      int pos = key.back() - '0';
      if (attribute=='h')
        FancyConsole::getInstance().hosts.at(pos).name = value;
      else if (attribute=='p')
        FancyConsole::getInstance().hosts.at(pos).port = value;
      else if (attribute=='f')
        FancyConsole::getInstance().hosts.at(pos).file = value;
    }
  }

  std::cout << "Content-type: text/html\r\n\r\n";

  std::cout << FancyConsole::getInstance().fancy_string();

  const auto &hosts = FancyConsole::getInstance().hosts;
  for (size_t i = 0; i < hosts.size(); ++i) {
    auto &h = hosts.at(i);
    if (h.exist()) {
      tcp::resolver::query q{h.name, h.port};
      std::make_shared<Client>("s" + std::to_string(i), h.file, std::move(q))
          ->resolve();
    }
  }

  service.run();

}