#ifndef NETWORKPROGRAMMING_COMMAND_H
#define NETWORKPROGRAMMING_COMMAND_H

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "simple_Pipes.h"
#include <sys/wait.h>
#include <utility>
#include <signal.h>
#include "simple_MyTypeDefines.h"
#include "simple_Node.h"
#include "simple_StringManipulator.h"

struct Command {
    virtual void execute(const String &usr_input_line) = 0;

    virtual ~Command() {
        redirection_info.pop_front();
        redirection_info.push_back(Node());
    }

protected:
    static Deque<Node> redirection_info;
};

struct ForkAndExec : public Command {
    ForkAndExec() {
        child_exit_handler.init_handler();
    }

    void execute(const String &line) override;

private:
    struct ChildExitHandler {
        void init_handler() { signal(SIGCHLD, handler); }

        static void handler(int signal_number) {
            int status;
            pid_t pid;
            do {
                pid = waitpid(-1, &status, WNOHANG);
            } while (pid > 0);
        }
    };

    static ChildExitHandler child_exit_handler;

    Vector<String> commands;

    pid_t fork_process(String command, size_t pipe_cnt, int input_stream, int output_stream, int error_stream);

    Vector<pid_t> pid_table;

    void open_numbered_pipe_to_destination(int destination, bool should_pipe_stderr) const;
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

struct PrintEnv : public Command {
    void execute(const String &usr_input_line) override;
};

struct SetEnv : public Command {
    void execute(const String &usr_input_line) override;
};

struct UnsetEnv : public Command {
    void execute(const String &usr_input_line) override {
        auto string_group = StringManipulator::split_and_trim(usr_input_line);
        unsetenv(string_group[1].c_str());
    }
};


#endif //NETWORKPROGRAMMING_COMMAND_H
