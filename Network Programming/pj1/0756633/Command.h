#ifndef NETWORKPROGRAMMING_COMMAND_H
#define NETWORKPROGRAMMING_COMMAND_H

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <utility>
#include <signal.h>
#include <boost/algorithm/string.hpp>
#include "MyTypeDefines.h"
#include "Node.h"

struct Command {
    // Abstract Class
    virtual void execute(const String &usr_input_line) = 0;

    virtual ~Command() {
        redirection_info.pop_front();
        Node node;
        redirection_info.push_back(node);
    }

protected:
    Vector<String> split_and_trim(const String &command, const String &predicate = "\t \n") {
        auto command_cpy(command);
        boost::trim(command_cpy);
        Vector<String> ret;
        boost::split(ret, command_cpy, boost::is_any_of(predicate), boost::token_compress_on);
        for (auto &item : ret) {
            /* trim white spaces on the start and on back of the string */
            boost::trim(item);
        }
        return std::move(ret);
    }

    String join(const Vector<String> &command) {
        String ret = boost::algorithm::join(command, " ");
        return ret;
    }

    static Deque<Node> redirection_info;
};

struct ForkAndExec : public Command {
    void execute(const String &usr_input_line) override;

private:
    Vector<String> commands_group;

    pid_t fork_process(String command, size_t pipe_cnt, int input_stream, int output_stream, int error_stream);

    static void child_handler(int signo) {
        /* Declare function as [static] to purge the hidden [this] pointer
         * C library does not know what the heck is this pointer.*/
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0) {

        }
    }

    int pipes_fd[55][2];

    Vector<pid_t> pid_table;

    String get_last_string(String p_str);
};

struct Exit : public Command {
    void execute(const String &usr_input_line) override {
        throw std::exception();
    }
};

struct EndOfFile : public Command {
    void execute(const String &usr_input_line) override {
        std::cout << "\n";
        throw std::exception();
    }
};

struct PressEnter : public Command {
    void execute(const String &usr_input_line) override { /* literally do nothing */ }
};

struct PrintEnv : public Command {
    void execute(const String &usr_input_line) override;
};

struct SetEnv : public Command {
    void execute(const String &usr_input_line) override;
};

struct Unknown : public Command {
    void execute(const String &usr_input_line) override;
};

#endif //NETWORKPROGRAMMING_COMMAND_H
