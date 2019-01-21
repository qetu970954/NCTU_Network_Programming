#include "simple_Command.h"


Deque<Node> Command::redirection_info(1500);
ForkAndExec::ChildExitHandler ForkAndExec::child_exit_handler{};
void ForkAndExec::execute(const String &line) {
    String line_copy = line;
    boost::trim(line_copy);

    int line_destination = 0;
    bool should_pipe_stderr = false;
    std::tie(line_destination, should_pipe_stderr) = StringManipulator::parse_and_get_numbered_pipe_info(line_copy);
    commands = StringManipulator::split_and_trim(line_copy, "|!");

    if (line_destination != 0) {
        commands.pop_back(); /* pop off |N, because it is useless for analyzing the entire commands. */
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

    for (int i = 0; i < commands.size();) {
        auto left_boundary = std::next(commands.begin(), i);
        auto right_boundary = (i + 50 < commands.size()) ? std::next(left_boundary, 50) : commands.end();
        i += std::distance(left_boundary, right_boundary);

        Vector<String> sub_commands;
        std::copy(left_boundary, right_boundary, std::back_inserter(sub_commands));

        if (right_boundary != commands.end()) {
            redirection_info.insert(redirection_info.begin() + 1, Node());
            open_numbered_pipe_to_destination(1, true);
        }

        size_t pipe_cnt = sub_commands.size() - 1;
        Pipes::open_pipes(pipe_cnt);

        for (size_t j = 0; j < sub_commands.size(); ++j) {
            int input_stream = (j == 0) ?
                               redirection_info.front().input_stream : Pipes::pipes_fd[j - 1][0];
            int output_stream = (j == sub_commands.size() - 1) ?
                                redirection_info.front().output_stream : Pipes::pipes_fd[j][1];
            int error_stream = STDERR_FILENO;
            if (should_pipe_stderr)
                error_stream = (j == sub_commands.size() - 1) ? redirection_info.front().error_stream
                                                              : Pipes::pipes_fd[j][1];

            pid_t c_pid = fork_process(sub_commands[j], pipe_cnt, input_stream, output_stream, error_stream);
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

        if (right_boundary != commands.end())
            redirection_info.pop_front();
    }

    pid_table.clear();
}

void ForkAndExec::open_numbered_pipe_to_destination(int destination, bool should_pipe_stderr) const {
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

pid_t
ForkAndExec::fork_process(String command, size_t pipe_cnt, int input_stream, int output_stream, int error_stream) {
    pid_t child_pid;
    while ((child_pid = fork()) < 0) {
        usleep(1000);
    }

    if (child_pid == 0) {
        if (input_stream != STDIN_FILENO) {
            if (dup2(input_stream, STDIN_FILENO))
                throw std::exception();

            if (input_stream == redirection_info[0].input_stream)
                close(redirection_info[0].pipe_write_end);
        }

        if (output_stream != STDOUT_FILENO) {
            if (dup2(output_stream, STDOUT_FILENO) == -1)
                throw std::exception();
        }

        if (error_stream != STDERR_FILENO) {
            if (dup2(error_stream, STDERR_FILENO) == -1)
                throw std::exception();
        }

        Pipes::close_pipes(pipe_cnt);

        Vector<String> space_split_command = StringManipulator::split_and_trim(command);
        if(space_split_command.size() > 2){
            if(space_split_command.at(space_split_command.size() - 2) == ">"){
                output_stream = open(space_split_command.back().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (dup2(output_stream, STDOUT_FILENO) == -1)
                    throw std::exception();
                close(output_stream);
                space_split_command.pop_back();
                space_split_command.pop_back();
            }
        }

        char *arg[space_split_command.size() + 1];
        for (auto i = 0; i < space_split_command.size(); ++i)
            arg[i] = const_cast<char *>(space_split_command[i].c_str());
        arg[space_split_command.size()] = nullptr;
        execvp(arg[0], arg);

        std::cerr << "Unknown command: [" << arg[0] << "]." << std::endl;
        exit(EXIT_FAILURE);
    }
    return child_pid;
}

void PrintEnv::execute(const String &usr_input_line) {
    auto string_group = StringManipulator::split_and_trim(usr_input_line);
    char *result = getenv(string_group[1].c_str()); // getenv might return a NULL
    if (result)
        std::cout << result << std::endl;
}

void SetEnv::execute(const String &usr_input_line) {
    auto string_group = StringManipulator::split_and_trim(usr_input_line);
    setenv(string_group[1].c_str(), string_group[2].c_str(), 1);
}
