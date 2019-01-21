#ifndef NETWORKPROGRAMMING_COMMAND_H
#define NETWORKPROGRAMMING_COMMAND_H

#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include "multi_proc_Pipes.h"
#include <sys/wait.h>
#include <utility>
#include <signal.h>
#include "multi_proc_MyTypeDefines.h"
#include "multi_proc_Node.h"
#include "multi_proc_StringManipulator.h"
#include <boost/format.hpp>
#include "multi_proc_PostOffice.h"
#include "multi_proc_SharedMemory.h"

struct Command {
    virtual void execute(const String &usr_input_line) = 0;

    virtual ~Command() {
        redirection_info.pop_front();
        Node node{};
        redirection_info.push_back(node);
    }

protected:
    static Deque<Node> redirection_info;
};

struct ForkAndExec : public Command {
    ForkAndExec() = default;

    void execute(const String &line) override;

private:
    pid_t fork_process(Vector<String> command_and_its_arguments, size_t pipe_cnt, int input_stream, int output_stream, int error_stream);

    void open_numbered_pipe_to_destination(int destination, bool should_pipe_stderr) const;

    bool parse_input_from_user_pipe(Vector<String> *commands, int *input_stream, Vector<int> *filefd_to_close, String line);

    bool parse_output_to_user_pipe(Vector<String> *commands, int *output_stream, Vector<int> *filefd_to_close, const String &line);

    void parse_output_to_file(Vector<String> *commands, int *output_stream);
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

struct Who : public Command {
    void execute(const String &usr_input_line) override {
        SharedMemory::get_instance().who();
    }
};

struct Name : public Command {
    void execute(const String &usr_input_line) override {
        SharedMemory::get_instance().name(StringManipulator::split_and_trim(usr_input_line).at(1));
    }
};

struct Yell : public Command {
    void execute(const String &usr_input_line) override {
        SharedMemory::get_instance().yell(usr_input_line.substr(5));
    }
};

struct Tell : public Command {
    void execute(const String &usr_input_line) override {
        String result = usr_input_line.substr(usr_input_line.find(' ')+1);
        int who = std::stoi(result.substr(0, result.find(' ')));
        String what = result.substr(result.find(' ')+1);
        SharedMemory::get_instance().tell(who, what);
    }
};

#endif //NETWORKPROGRAMMING_COMMAND_H
