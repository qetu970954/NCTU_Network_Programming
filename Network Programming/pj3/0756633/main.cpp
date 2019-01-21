#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/format.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

using boost::asio::ip::tcp;
using namespace std;

boost::asio::io_service io_service;

struct PanelCgi final {
  static string form_string() {
    string ret =
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "  <head>\n"
        "    <title>NP Project 3 Panel</title>\n"
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
        "href=\"https://cdn4.iconfinder.com/data/icons/iconsimple-setting-time/"
        "512/dashboard-512.png\"\n"
        "    />\n"
        "    <style>\n"
        "      * {\n"
        "        font-family: 'Source Code Pro', monospace;\n"
        "      }\n"
        "    </style>\n"
        "  </head>\n"
        "  <body class=\"bg-secondary pt-5\">"
        "<form action=\"console.cgi\" method=\"GET\">\n"
        "      <table class=\"table mx-auto bg-light\" style=\"width: "
        "inherit\">\n"
        "        <thead class=\"thead-dark\">\n"
        "          <tr>\n"
        "            <th scope=\"col\">#</th>\n"
        "            <th scope=\"col\">Host</th>\n"
        "            <th scope=\"col\">Port</th>\n"
        "            <th scope=\"col\">Input File</th>\n"
        "          </tr>\n"
        "        </thead>\n"
        "        <tbody>";

    boost::format fmt(
        "          <tr>\n"
        "            <th scope=\"row\" class=\"align-middle\">Session "
        "%1%</th>\n"
        "            <td>\n"
        "              <div class=\"input-group\">\n"
        "                <select name=\"h%3%\" class=\"custom-select\">\n"
        "                  <option></option>{%2%}\n"
        "                </select>\n"
        "                <div class=\"input-group-append\">\n"
        "                  <span "
        "class=\"input-group-text\">.cs.nctu.edu.tw</span>\n"
        "                </div>\n"
        "              </div>\n"
        "            </td>\n"
        "            <td>\n"
        "              <input name=\"p%3%\" type=\"text\" "
        "class=\"form-control\" size=\"5\" />\n"
        "            </td>\n"
        "            <td>\n"
        "              <select name=\"f%3%\" class=\"custom-select\">\n"
        "                <option></option>\n"
        "               %4%\n"
        "              </select>\n"
        "            </td>\n"
        "          </tr>");

    for (int i = 0; i < 5; ++i) {
      ret += (fmt % (i + 1) % host_menu() % i % test_case_menu()).str();
    }

    ret +=
        "          <tr>\n"
        "            <td colspan=\"3\"></td>\n"
        "            <td>\n"
        "              <button type=\"submit\" class=\"btn btn-info "
        "btn-block\">Run</button>\n"
        "            </td>\n"
        "          </tr>\n"
        "        </tbody>\n"
        "      </table>\n"
        "    </form>\n"
        "  </body>\n"
        "</html>";
    return ret;
  }

 private:
  static string host_menu() {
    vector<string> hosts_menu = {
        "<option value=\"nplinux1.cs.nctu.edu.tw\">nplinux1</option>",
        "<option value=\"nplinux2.cs.nctu.edu.tw\">nplinux2</option>",
        "<option value=\"nplinux3.cs.nctu.edu.tw\">nplinux3</option>",
        "<option value=\"nplinux4.cs.nctu.edu.tw\">nplinux4</option>",
        "<option value=\"nplinux5.cs.nctu.edu.tw\">nplinux5</option>",
        "<option value=\"npbsd1.cs.nctu.edu.tw\">npbsd1</option>",
        "<option value=\"npbsd2.cs.nctu.edu.tw\">npbsd2</option>",
        "<option value=\"npbsd3.cs.nctu.edu.tw\">npbsd3</option>",
        "<option value=\"npbsd4.cs.nctu.edu.tw\">npbsd4</option>",
        "<option value=\"npbsd5.cs.nctu.edu.tw\">npbsd5</option>"};

    return string(boost::algorithm::join(hosts_menu, ""));
  }

  static string test_case_menu() {
    vector<string> test_case_menu = {
        "<option value=\"t1.txt\">t1.txt</option>",
        "<option value=\"t2.txt\">t2.txt</option>",
        "<option value=\"t3.txt\">t3.txt</option>",
        "<option value=\"t4.txt\">t4.txt</option>",
        "<option value=\"t5.txt\">t5.txt</option>",
        "<option value=\"t6.txt\">t6.txt</option>",
        "<option value=\"t7.txt\">t7.txt</option>",
        "<option value=\"t8.txt\">t8.txt</option>",
        "<option value=\"t9.txt\">t9.txt</option>",
        "<option value=\"t10.txt\">t10.txt</option>"};

    return string(boost::algorithm::join(test_case_menu, ""));
  }
};

class FancyConsole final {
 public:
  static FancyConsole& getInstance() {
    static FancyConsole instance;
    return instance;
  }

  FancyConsole(const FancyConsole&) = delete;

  void operator=(const FancyConsole&) = delete;

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

 private:
  FancyConsole() = default;
};

class Appender final {
 public:
  string output_shell(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt(
        "<script>document.getElementById('%1%').innerHTML += '%2%';</script>");
    return (fmt % session % content).str();
  }

  string output_command(std::string session, std::string content) {
    encode(content);
    boost::replace_all(content, "\n", "&#13;");
    boost::replace_all(content, "\r", "");
    boost::format fmt(
        "<script>document.getElementById('%1%').innerHTML += "
        "'<b>%2%</b>';</script>");
    return (fmt % session % content).str();
  }

 private:
  void encode(std::string& data) {
    using boost::algorithm::replace_all;
    replace_all(data, "&", "&amp;");
    replace_all(data, "\"", "&quot;");
    replace_all(data, "\'", "&apos;");
    replace_all(data, "<", "&lt;");
    replace_all(data, ">", "&gt;");
  }
};

struct Parser final {
  static Parser& getInstance() {
    static Parser instance;
    return instance;
  }

  Parser(const Parser&) = delete;

  void operator=(const Parser&) = delete;

  static vector<string> split_and_trim(string cmd,
                                       const string& predicate = "\t\n ") {
    vector<string> ret;
    cmd = boost::trim_copy(cmd);
    boost::split(ret, cmd, boost::is_any_of(predicate),
                 boost::token_compress_on);
    for (auto& item : ret) boost::trim(item);
    return ret;
  }

  map<string, string> parse_http_request(string request) const {
    vector<string> http_request;

    for (auto lhs = request.begin(), rhs = request.begin();
         rhs != request.end(); ++rhs) {
      if (*rhs == '\n') {
        string line(lhs, rhs);
        if (line.back() == '\r') line.pop_back();
        http_request.push_back(line);
        lhs = rhs;
      }
    }

    map<string, string> parser;

    for (auto line : http_request) {
      auto key = line.substr(0, line.find(':'));
      auto value = line.substr(key.size());
      if (key != line) {
        value = value.substr(2);
        if (key.front() == '\n') key = key.substr(1);
        parser.insert({key, value});
      } else {
        trim_if(line, boost::is_any_of("\n\r"));
        if (line.empty()) continue;

        auto header = split_and_trim(line);

        for (auto stuff : header) {
          std::cout << stuff << ' ';
        }

        auto METHOD = header.at(0);
        parser.insert({"METHOD", METHOD});

        auto URI = header.at(1);
        parser.insert({"URI", URI});

        auto target = URI.substr(0, URI.find('?'));
        parser.insert({"TARGET", target});
        if (URI != target) {
          auto query = URI.substr(target.size() + 1);
          parser.insert({"QUERY_STRING", query});
        }
      }
    }

    return parser;
  }

 private:
  Parser() = default;
};

class Session : public std::enable_shared_from_this<Session> {
 public:
  class Client final : public std::enable_shared_from_this<Client> {
   public:
    Client(const shared_ptr<Session>& ptr, string session, string file)
        : ptr_(ptr),
          session_(session),
          file_("test_case/" + file, std::ios::in) {
      if (!file_.is_open())
        appender_.output_command(session, "DAMN file not open!");
    }

    void resolve(string host, string port) {
      auto self(shared_from_this());

      tcp::resolver::query query_{host, port};

      resolver.async_resolve(query_,
                             [this, self](const boost::system::error_code& ec,
                                          tcp::resolver::iterator it) {
                               if (!ec) {
                                 cout << "Resolve complete!" << endl;
                                 connect(it);
                               }
                             });
    }

   private:
    void connect(const tcp::resolver::iterator it) {
      auto self(shared_from_this());

      server_socket.async_connect(*it,
                                  [this, self](boost::system::error_code ec) {
                                    if (!ec) {
                                      cout << "Connect complete!" << endl;
                                      read();
                                    }
                                  });
    }

    void read() {
      auto self(shared_from_this());

      server_socket.async_receive(
          boost::asio::buffer(bytes_),
          [this, self](boost::system::error_code ec, std::size_t length) {
            if (!ec) {
              std::string data(bytes_.begin(), bytes_.begin() + length);

              boost::asio::write(
                  ptr_->socket_,
                  boost::asio::buffer(appender_.output_shell(session_, data)));

              if (data.find("% ") != std::string::npos) {
                std::string command;
                getline(file_, command);
                cout << "The command is " << command << endl;
                boost::asio::write(ptr_->socket_,
                                   boost::asio::buffer(appender_.output_command(
                                       session_, command + '\n')));
                server_socket.write_some(boost::asio::buffer(command + '\n'));
              }
              read();
            }
          });
    }

    shared_ptr<Session> ptr_;

    Appender appender_;
    std::string session_;  // e.g. "s0"
    std::fstream file_;    // e.g. "t1.txt"

    tcp::resolver resolver{io_service};
    tcp::socket server_socket{io_service};
    std::array<char, 4096> bytes_ = {};
  };

  Session(tcp::socket socket) : socket_(std::move(socket)) {}

  void start() { do_read(); }

 private:
  void do_read() {
    auto self(shared_from_this());
    socket_.async_read_some(
        boost::asio::buffer(data_, max_length),
        [this, self](boost::system::error_code ec, std::size_t length) {
          if (!ec) {
            auto result = Parser::getInstance().parse_http_request(
                string(data_, data_ + length));

            std::cout << "------------------------------------------------\n";

            auto target = result["TARGET"];
            string s;
            if (target == "/panel.cgi") {
              s = get_panel_cgi_write_string();
              do_write(s);
            } else if (target == "/console.cgi") {
              s = get_console_cgi_write_string(result["QUERY_STRING"]);
              do_write(s);
              const auto& hosts = FancyConsole::getInstance().hosts;
              for (size_t i = 0; i < hosts.size(); ++i) {
                auto& h = hosts.at(i);
                if (h.exist()) {
                  std::make_shared<Client>(shared_from_this(),
                                           "s" + std::to_string(i), h.file)
                      ->resolve(h.name, h.port);
                }
              }
            }
          }
        });
  }

  void do_write(string s) {
    auto self(shared_from_this());
    boost::asio::async_write(
        socket_, boost::asio::buffer(s.c_str(), s.length()),
        [this, self](boost::system::error_code ec, std::size_t /*length*/) {
          if (!ec) {
          }
        });
  }

  string get_panel_cgi_write_string() {
    auto self(shared_from_this());
    string the_string_to_write =
        "HTTP/1.1 200 OK\r\n"
        "Content-type: text/html\r\n\r\n";
    the_string_to_write += PanelCgi::form_string();
    return the_string_to_write;
  }

  string get_console_cgi_write_string(string query_string) {
    auto self(shared_from_this());

    for (auto s : Parser::split_and_trim(query_string, "&")) {
      auto key = s.substr(0, s.find('='));
      auto value = s.substr(key.size() + 1);

      if (!value.empty()) {
        char attribute = key.front();
        int pos = key.back() - '0';
        if (attribute == 'h')
          FancyConsole::getInstance().hosts.at(pos).name = value;
        else if (attribute == 'p')
          FancyConsole::getInstance().hosts.at(pos).port = value;
        else if (attribute == 'f')
          FancyConsole::getInstance().hosts.at(pos).file = value;
      }
    }

    string the_string_to_write =
        "HTTP/1.1 200 OK\r\n"
        "Content-type: text/html\r\n\r\n";
    the_string_to_write += FancyConsole::getInstance().fancy_string();
    return the_string_to_write;
  }

  tcp::socket socket_;

  enum { max_length = 4096 };

  char data_[max_length];
};

class Server {
 public:
  Server(short port) : acceptor_(io_service, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
  }

 private:
  void do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
          if (!ec) {
            std::make_shared<Session>(std::move(socket))->start();
          }

          do_accept();
        });
  }

  tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "Usage: http_server <port>\n";
      return 1;
    }

    Server s(std::atoi(argv[1]));

    io_service.run();
  } catch (std::exception& e) {
    std::cerr << "Exception: " << e.what() << "\n";
  }

  return 0;
}
