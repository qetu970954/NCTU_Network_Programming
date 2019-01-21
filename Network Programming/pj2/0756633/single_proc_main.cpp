#include <utility>
#include "single_proc_MyTypeDefines.h"
#include "single_proc_StringManipulator.h"
#include <iostream>
#include "single_proc_Pipes.h"
#include "single_proc_Node.h"
#include "fcntl.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <algorithm>
#include <sys/socket.h>
#include <fcntl.h>
#include "single_proc_MyTypeDefines.h"

const int MAX_USERS = 35;

struct GLOBAL {
    static Set<int> open_sockets;

    static ssize_t send(int socket_fd, const std::string &message) {
        return ::send(socket_fd, message.c_str(), message.size(), 0);
    }

    static void broadcast(const std::string &message) {
        for (auto socket : open_sockets) {
            GLOBAL::send(socket, message);
        }
    }
};

class PostOffice {
    struct UserPipe {
        int pipeRead = 0;
        int pipeWrite = 0;

        void generate_pipe() {
            int fd[2] = {};
            if (pipe(fd))
                perror("Generating Pipe Failed");
            pipeRead = fd[0];
            pipeWrite = fd[1];
        }

        void close_read_head() {
            close(pipeRead);
        }
    };

private:
    PostOffice() = default;

    Queue<UserPipe> user_pipe_to_remove;

    Vector<Vector<Pair<int, UserPipe>>> opened_user_pipe = Vector<Vector<Pair<int, UserPipe>>>(MAX_USERS);

    const String path = "user_pipe/";

    String form_string(int sender, int receiver) {
        return "#" + std::to_string(sender) + "->#" + std::to_string(receiver);
    }

public:
    static PostOffice &get_instance() {
        static PostOffice instance;
        return instance;
    }

    void delete_all_files_associate_to(int uid) {
        for (Vector<Pair<int, UserPipe>> &user_pipes : opened_user_pipe) {
            for (auto iter = user_pipes.begin(); iter != user_pipes.end();) {
                if (iter->first == uid) {
                    iter->second.close_read_head();
                    user_pipes.erase(iter);
                } else
                    ++iter;
            }
        }

        for (auto &user_pipe :  opened_user_pipe.at(uid)) {
            user_pipe.second.close_read_head();
        }

        opened_user_pipe.at(uid).clear();
    }

    void delete_user_pipes_in_queue() {
        while (!user_pipe_to_remove.empty()) {
            user_pipe_to_remove.front().close_read_head();
            user_pipe_to_remove.pop();
        }
    }

    int erase(int sender_uid, int receiver_uid) {
        String name = form_string(sender_uid, receiver_uid);
        auto search_result = std::find_if(opened_user_pipe.at(sender_uid).begin(), opened_user_pipe.at(sender_uid).end(), [=](auto pair) { return pair.first == receiver_uid; });

        if (search_result == opened_user_pipe.at(sender_uid).end())
            throw std::runtime_error("*** Error: the pipe " + name + " does not exist yet. ***");

        int pipe_read_head = search_result->second.pipeRead;
        user_pipe_to_remove.push(search_result->second);
        opened_user_pipe.at(sender_uid).erase(search_result);
        // We need to wait until the child process finish all its work then we can close it.
        return pipe_read_head;
    }

    int insert(int sender_uid, int receiver_uid) {
        String name = form_string(sender_uid, receiver_uid);
        auto search_result = std::find_if(opened_user_pipe.at(sender_uid).begin(), opened_user_pipe.at(sender_uid).end(), [=](auto pair) { return pair.first == receiver_uid; });

        if (search_result != opened_user_pipe.at(sender_uid).end())
            throw std::runtime_error("*** Error: the pipe " + name + " already exists. ***");

        UserPipe userPipe;
        userPipe.generate_pipe();
        opened_user_pipe.at(sender_uid).push_back(std::make_pair(receiver_uid, userPipe));
        return userPipe.pipeWrite;
    }
};

struct Client {
    explicit Client(int socket_fd = -1, String ip = "Unset", uint16_t port = 0, const String &name = "None", bool connection_status = false, int user_id = -1) :
            socket_fd(socket_fd),
            ip(std::move(ip)),
            port(port),
            name(name),
            connection_status(connection_status),
            user_id(user_id) {
        redirection_info = Deque<Node>(2000);
        env_table.clear();
        env_table.emplace("PATH", "bin:.");
    }

    int socket_fd;
    String ip;
    uint16_t port;
    String name;
    bool connection_status;
    int user_id; // Client's position in Server.


    Deque<Node> redirection_info;
    Map<String, String> env_table;

    friend std::ostream &operator<<(std::ostream &os, const Client &client) {
        os << " socket_fd: " << client.socket_fd
           << " user_id: " << client.user_id
           << " ip: " << client.ip
           << " name: " << client.name
           << " port: " << client.port;
        return os;
    }

    void reset() {
        *this = Client();
    }

    void run_shell_command(Vector<String> command_group) {

    }

    String get_ip_and_port() {
        return "CGILAB/511";
        // return ip + "/" + std::to_string(port);
    }

    int stdfd[3]{};

    void stash_fds() {
        stdfd[0] = dup(STDIN_FILENO);
        stdfd[1] = dup(STDOUT_FILENO);
        stdfd[2] = dup(STDERR_FILENO);
    };

    void restore_fds() {
        dup2(stdfd[0], STDIN_FILENO);
        close(stdfd[0]);
        dup2(stdfd[1], STDOUT_FILENO);
        close(stdfd[1]);
        dup2(stdfd[2], STDERR_FILENO);
        close(stdfd[2]);
    }

    void fork_and_exec(const String &);

    void open_numbered_pipe_to_destination(int destination, bool should_pipe_stderr);

    pid_t fork_process(const Vector<String> &commands, size_t pipe_cnt, int input_stream, int output_stream, int error_stream);

    void parse_output_to_file(Vector<String> *commands, int *output_stream);

    bool parse_output_to_user_pipe(Vector<String> *commands, int *output_stream, Vector<int> *files_to_close, const String &line);

    bool parse_input_from_user_pipe(Vector<String> *commands, int *input_stream, Vector<int> *files_to_close, const String &line);
};

// ------------------------------------- globals -------------------------------------
Set<int> GLOBAL::open_sockets;

Vector<Client> clients = Vector<Client>(MAX_USERS);

// ------------------------------------- functions -------------------------------------
void execute(int socket);

void insert_new_client_and_its_user_id(Client c);

Vector<Client>::iterator get_client_by_fd(int fd);

Vector<Client>::iterator get_client_by_uid(int uid);

void print_clients();

void who(Vector<Client>::iterator iter);

void tell(Vector<Client>::iterator source, int target_uid, String message);

void name(Vector<Client>::iterator client_iter, String name);

int main(int argc, char *argv[]) {
    if(argc < 2){
        std::cerr << "Please input port" << std::endl;
        return -1;
    }


    signal(SIGCHLD, SIG_IGN);

    fd_set master; // master file descriptor 清單
    fd_set read_fds; // 給 select() 用的暫時 file descriptor 清單
    FD_ZERO(&master); // 清除 master 與 temp sets
    FD_ZERO(&read_fds);

    sockaddr_in sockaddr_client{}; // client address
    socklen_t address_len;
    addrinfo hints{}, *addrinfo_ptr, *p;

    // 給我們一個 socket，並且 bind 它
    memset(&hints, 0, sizeof hints);

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int rv = getaddrinfo(nullptr, argv[1], &hints, &addrinfo_ptr);
    if (rv != 0) {
        fprintf(stderr, "selectserver: %s\n", gai_strerror(rv));
        exit(1);
    }

    int listener = 0;
    for (p = addrinfo_ptr; p != nullptr; p = p->ai_next) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (listener < 0) {
            continue;
        }
        int option = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

        if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
            close(listener);
            continue;
        }
        break;
    }

    if (p == nullptr) {
        perror("selectserver: failed to bind\n");
        exit(2);
    }
    freeaddrinfo(addrinfo_ptr);

    // listen
    if (listen(listener, 200) == -1) {
        perror("listen");
        exit(3);
    }

    // 將 listener 新增到 master set
    FD_SET(listener, &master);

    // 持續追蹤最大的 file descriptor
    int fd_max = listener; // 最大的 file descriptor 數目

    // 主要迴圈
    for (;;) {
        read_fds = master; // 複製 master
        if (select(fd_max + 1, &read_fds, nullptr, nullptr, nullptr) == -1) {
            perror("select");
            exit(4);
        }

        // 在現存的連線中尋找需要讀取的資料
        for (int fd = 0; fd <= fd_max; fd++) {
            if (FD_ISSET(fd, &read_fds)) { // 我們找到一個！！
                if (fd == listener) {
                    // handle new connections
                    address_len = sizeof sockaddr_client;
                    int new_fd = accept(listener, (struct sockaddr *) &sockaddr_client, &address_len);
                    if (new_fd < 0) {
                        perror("accept");
                        exit(4);
                    } else {
                        FD_SET(new_fd, &master); // 新增到 master set
                        fd_max = std::max(fd_max, new_fd);

                        Client client(new_fd,
                                      inet_ntoa(sockaddr_client.sin_addr),
                                      ntohs(sockaddr_client.sin_port),
                                      "(no name)", true);

                        insert_new_client_and_its_user_id(client);
                        String welcome = "****************************************\n"
                                         "** Welcome to the information server. **\n"
                                         "****************************************\n";
                        GLOBAL::send(new_fd, welcome);
                        String msg = "*** User '" + client.name + "' entered from " + client.get_ip_and_port() + ". ***\n";
                        GLOBAL::broadcast(msg);
                        GLOBAL::send(new_fd, "% ");
                    }
                } else {

                    execute(fd);
                    auto client_ptr = get_client_by_fd(fd);
                    if (client_ptr->connection_status == 0) {
                        String msg = "*** User '" + client_ptr->name + "' left. ***\n";
                        GLOBAL::broadcast(msg);
                        GLOBAL::open_sockets.erase(fd);
                        PostOffice::get_instance().delete_all_files_associate_to(client_ptr->user_id);
                        client_ptr->reset();
                        close(fd);
                        FD_CLR(fd, &master);
                    } else {
                        GLOBAL::send(fd, "% ");
                    }
                } // END handle data from client
            } // END got new incoming connection
        } // END looping through file descriptors
    } // END for( ; ; )--and you thought it would never end!
}

void insert_new_client_and_its_user_id(Client c) {
    auto ptr = std::find_if(clients.begin(), clients.end(), [](auto b) { return b.connection_status == false; });
    *ptr = c;
    ptr->user_id = std::distance(clients.begin(), ptr) + 1;
    GLOBAL::open_sockets.insert(c.socket_fd);
}

Vector<Client>::iterator get_client_by_fd(int fd) {
    auto iter = std::find_if(clients.begin(), clients.end(), [fd](auto p) { return p.socket_fd == fd; });
    return iter;
}

Vector<Client>::iterator get_client_by_uid(int uid) {
    auto iter = std::find_if(clients.begin(), clients.end(), [uid](auto p) { return p.user_id == uid; });
    return iter;
}

void who(const Vector<Client>::iterator iter) {
    std::cout << std::left << "<ID>" << "\t" << "<nickname>" << "\t" << "<IP/port>" << "\t" << "<indicate me>" << std::endl;
    for (size_t i = 0; i < clients.size(); ++i) {
        if (clients.at(i).connection_status) {
            std::cout << std::left << clients[i].user_id << "\t" << clients[i].name << "\t" << clients.at(i).get_ip_and_port();
            if (iter == clients.begin() + i)
                std::cout << "\t<-me";
            std::cout << std::endl;
        }
    }
}

void tell(const Vector<Client>::iterator source, int target_uid, String message) {
    Client target = clients.at(target_uid - 1);
    if (target.connection_status) {
        message = "*** " + source->name + " told you ***: " + message + "\n";
        GLOBAL::send(target.socket_fd, message);
    } else {
        std::cout << "*** Error: user #" << target_uid << " does not exist yet. ***" << std::endl;
    }
}

void name(const Vector<Client>::iterator client_iter, String name) {
    bool exist_duplicate_name = false;
    for (const auto &i : clients) {
        if (i.connection_status && i.name == name) {
            exist_duplicate_name = true;
            break;
        }
    }
    if (exist_duplicate_name) {
        std::cout << "*** User '" << name << "' already exists. ***" << std::endl;
    } else {
        client_iter->name = name;
        String str = "*** User from " + client_iter->get_ip_and_port() + " is named '" + client_iter->name + "'. ***\n";
        GLOBAL::broadcast(str);
    }
}

void Client::fork_and_exec(const String &line) {
    String line_copy = line;
    boost::trim(line_copy);

    int line_destination = 0;
    bool should_pipe_stderr = false;
    std::tie(line_destination, should_pipe_stderr) = StringManipulator::parse_and_get_numbered_pipe_info(line_copy);
    Vector<String> cmds_split_by_vertical = StringManipulator::split_and_trim(line_copy, "|!");

    if (line_destination != 0) {
        cmds_split_by_vertical.pop_back(); /* pop off |N, because it is useless for analyzing the entire commands. */
        open_numbered_pipe_to_destination(line_destination, should_pipe_stderr);
    }

    /* When facing super long commands, we can encounter some issues such as [not enough pipe] or [unable to fork].
     * Thus, I simply divide them into sub commands and use numbered pipe with N=1 to concatenate them.
     *
     * e.g. ls | cat | cat | ... | cat ,
     *      will become
     *      ls | cat | cat |1
     *      cat | cat
     */

    for (int i = 0; i < cmds_split_by_vertical.size();) {
        Vector<pid_t> pid_table;
        auto left_boundary = std::next(cmds_split_by_vertical.begin(), i);
        auto right_boundary = (i + 50 < cmds_split_by_vertical.size()) ? std::next(left_boundary, 50) : cmds_split_by_vertical.end();
        i += std::distance(left_boundary, right_boundary);

        Vector<String> sliced_cmd_by_vertical;
        std::copy(left_boundary, right_boundary, std::back_inserter(sliced_cmd_by_vertical));

        if (right_boundary != cmds_split_by_vertical.end()) {
            redirection_info.insert(redirection_info.begin() + 1, Node());
            open_numbered_pipe_to_destination(1, true);
        }

        size_t pipe_cnt = sliced_cmd_by_vertical.size() - 1;
        Pipes::open_pipes(pipe_cnt);

        Vector<int> filefd_to_close;
        for (size_t j = 0; j < sliced_cmd_by_vertical.size(); ++j) {
            int input_stream = (j == 0) ? redirection_info.front().input_stream : Pipes::pipes_fd[j - 1][0];
            int output_stream = (j == sliced_cmd_by_vertical.size() - 1) ? redirection_info.front().output_stream : Pipes::pipes_fd[j][1];
            int error_stream = STDERR_FILENO;
            if (should_pipe_stderr)
                error_stream = (j == sliced_cmd_by_vertical.size() - 1) ? redirection_info.front().error_stream : Pipes::pipes_fd[j][1];

            Vector<String> space_splited = StringManipulator::split_and_trim(sliced_cmd_by_vertical[j]);

            if (!parse_input_from_user_pipe(&space_splited, &input_stream, &filefd_to_close, line))
                return;

            parse_output_to_file(&space_splited, &output_stream);
            if (!parse_output_to_user_pipe(&space_splited, &output_stream, &filefd_to_close, line))
                return;


            pid_t c_pid = fork_process(space_splited, pipe_cnt, input_stream, output_stream, error_stream);
            pid_table.push_back(c_pid);

            if (input_stream == redirection_info.front().input_stream && input_stream != STDIN_FILENO) {
                close(redirection_info.front().pipe_write_end);
                close(redirection_info.front().input_stream);
            };
        }

        Pipes::close_pipes(pipe_cnt);

        if (redirection_info.front().output_stream != STDOUT_FILENO) {
            for (auto first = pid_table.begin(); std::distance(first, pid_table.end()) < pid_table.size() - 1; ++first)
                waitpid(*first, nullptr, 0);
        } else {
            for (auto item : pid_table)
                waitpid(item, nullptr, 0);
        }

        if (right_boundary != cmds_split_by_vertical.end())
            redirection_info.pop_front();

        for (auto fd : filefd_to_close)
            close(fd);
    }
    PostOffice::get_instance().delete_user_pipes_in_queue();
}

void Client::open_numbered_pipe_to_destination(int destination, bool should_pipe_stderr) {
    auto &target_command = redirection_info[destination];
    auto &current_command = redirection_info.front();
    bool target_is_default = target_command.input_stream == STDIN_FILENO;

    if (target_is_default) {
        int pipe_fd[2];
        pipe(pipe_fd);
        enum {
            READ_HEAD, WRITE_HEAD
        };
        target_command.input_stream = pipe_fd[READ_HEAD];
        target_command.pipe_write_end = pipe_fd[WRITE_HEAD];
        current_command.output_stream = pipe_fd[WRITE_HEAD];
        if (should_pipe_stderr)
            current_command.error_stream = pipe_fd[WRITE_HEAD];
    } else {
        current_command.output_stream = target_command.pipe_write_end;
    }
}

pid_t Client::fork_process(const Vector<String> &commands, size_t pipe_cnt, int input_stream, int output_stream, int error_stream) {
    pid_t child_pid;
    while ((child_pid = fork()) < 0) {
        usleep(10000);
    }

    if (child_pid == 0) {
        if (input_stream != STDIN_FILENO) {
            if (dup2(input_stream, STDIN_FILENO))
                perror("dup2 stdin!");
            if (input_stream == redirection_info[0].input_stream)
                close(redirection_info[0].pipe_write_end);
        }

        if (output_stream != STDOUT_FILENO) {
            if (dup2(output_stream, STDOUT_FILENO) == -1)
                perror("dup2 stdout!");
        }

        if (error_stream != STDERR_FILENO) {
            if (dup2(error_stream, STDERR_FILENO) == -1)
                perror("dup2 stderr!");
        }

        Pipes::close_pipes(pipe_cnt);

        char *arg[commands.size() + 1];
        for (auto i = 0; i < commands.size(); ++i)
            arg[i] = const_cast<char *>(commands[i].c_str());
        arg[commands.size()] = nullptr;
        execvp(arg[0], arg);
        std::cerr << "Unknown command: [" << arg[0] << "]." << std::endl;
        std::cerr.flush();
        exit(EXIT_FAILURE);
    }
    return child_pid;
}

void Client::parse_output_to_file(Vector<String> *commands, int *output_stream) {
    if (commands->size() > 2) {
        if (commands->at(commands->size() - 2) == ">") {
            *output_stream = open(commands->back().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            commands->pop_back();
            commands->pop_back();
        }
    }
}

bool Client::parse_input_from_user_pipe(Vector<String> *commands, int *input_stream, Vector<int> *files_to_close, const String &line) {
    for (auto iter = commands->begin(); iter != commands->end();) {
        if (iter->front() == '<' && iter->size() >= 2) {
            String sender_uid = iter->substr(1);
            String receiver_uid = std::to_string(this->user_id);
            auto sender_client_iter = get_client_by_uid(stoi(sender_uid));

            if (sender_client_iter == clients.end()) {
                String msg = "*** Error: user #" + sender_uid + " does not exist yet. ***";
                std::cout << msg << std::endl;
                return false;
            }

            int result;
            try {
                result = PostOffice::get_instance().erase(sender_client_iter->user_id, this->user_id);
            } catch (std::exception &e) {
                std::cout << e.what() << std::endl;
                return false;
            }

            if (result == -1) {
                perror("Error opening file");
                return false;
            }

            *input_stream = result;
            files_to_close->push_back(result);
            String msg = "*** " + this->name + " (#" + receiver_uid + ") just received from " +
                         sender_client_iter->name + " (#" + sender_uid + ") by '" + line + "' ***\n";
            GLOBAL::broadcast(msg);
            commands->erase(iter);
            break;
        } else {
            ++iter;
        }
    }
    return true;
}

bool Client::parse_output_to_user_pipe(Vector<String> *commands, int *output_stream, Vector<int> *files_to_close, const String &line) {
    for (auto iter = commands->begin(); iter != commands->end();) {
        if (iter->front() == '>' && iter->size() >= 2) {
            String sender_uid = std::to_string(this->user_id);
            String receiver_uid = iter->substr(1);
            auto receiver_client_iter = get_client_by_uid(stoi(receiver_uid));

            if (receiver_client_iter == clients.end()) {
                String msg = "*** Error: user #" + receiver_uid + " does not exist yet. ***";
                std::cout << msg << std::endl;
                return false;
            }

            int result;
            try {
                result = PostOffice::get_instance().insert(this->user_id, receiver_client_iter->user_id);
            } catch (std::exception &e) {
                std::cout << e.what() << std::endl;
                return false;
            }

            if (result == -1) {
                perror("Error creating file");
                return false;
            }

            *output_stream = result;
            files_to_close->push_back(result);

            String msg = "*** " + this->name + " (#" + sender_uid + ") just piped '" + line +
                         "' to " + receiver_client_iter->name + " (#" + receiver_uid + ") ***\n";

            GLOBAL::broadcast(msg);
            commands->erase(iter);
            break;
        } else {
            ++iter;
        }
    }
    return true;
}

void execute(int socket) {
    auto client_iter = get_client_by_fd(socket);
    clearenv();
    for(auto i : client_iter->env_table){
        setenv(i.first.c_str(), i.second.c_str(), 1);
    }

    client_iter->stash_fds();

    dup2(socket, STDIN_FILENO);
    dup2(socket, STDOUT_FILENO);
    dup2(socket, STDERR_FILENO);

    String input_string;
    auto& client_env_table = client_iter->env_table;

    std::getline(std::cin, input_string);
    if(input_string.back() == '\r')
        input_string.pop_back();

    bool no_need_to_pop_front = false;
    if(input_string.substr(0,4) == "exit"){
        client_iter->connection_status = false;
    }else if (input_string.empty() || input_string == "\r") {
        no_need_to_pop_front = true;
    }
    else if (std::cin.fail()){
        client_iter->connection_status = false;
    }else if(input_string.substr(0,9) == "printenv "){
        auto result = client_env_table.find(input_string.substr(9));
        if (result != client_env_table.end()) {
            std::cout << result->second << std::endl;
        }
    }else if(input_string.substr(0,7) == "setenv ") {
        auto string_group = StringManipulator::split_and_trim(input_string.substr(6));
        auto result = client_env_table.find(string_group.at(0));
        if (result != client_env_table.end())
            client_env_table.at(string_group.at(0)) = string_group.at(1);
        else
            client_env_table.emplace(string_group.at(0), string_group.at(1));
    }else if (input_string.substr(0, 3) == "who")
        who(client_iter);
    else if (input_string.substr(0, 5) == "name ")
        name(client_iter, input_string.substr(5));
    else if (input_string.substr(0, 5) == "yell ") {
        String prefix = "*** " + client_iter->name + " yelled ***: ";
        GLOBAL::broadcast(prefix + input_string.substr(5) + "\n");
    }
    else if (input_string.substr(0, 5) == "tell "){
        String result = input_string.substr(input_string.find(' ')+1);
        int who = std::stoi(result.substr(0, result.find(' ')));
        String what = result.substr(result.find(' ')+1);
        tell(client_iter, who, what);
    }
    else {
        client_iter->fork_and_exec(input_string);
    }

    if(!no_need_to_pop_front) {
        client_iter->redirection_info.pop_front();
        client_iter->redirection_info.push_back(Node());
    }

    std::cout.flush();
    std::cerr.flush();

    client_iter->restore_fds();
}
