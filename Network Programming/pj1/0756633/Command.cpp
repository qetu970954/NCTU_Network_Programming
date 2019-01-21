#include <memory>
#include "Command.h"

Deque<Node> Command::redirection_info(1500);

void ForkAndExec::execute(const String &usr_input_line) {
    String usr_input_copy = usr_input_line;
    boost::trim(usr_input_copy);
    String last_string = get_last_string(usr_input_copy);

    int command_destination = 0;
    bool pipe_stderr = false;
    if (last_string[0] == '|' || last_string[0] == '!') {
        usr_input_copy = usr_input_copy.substr(0, usr_input_copy.size() - last_string.size());
        command_destination = std::stoi(last_string.substr(1, last_string.size() - 1));
        if (last_string.front() == '!') {
            pipe_stderr = true;
        }
    }

    if (command_destination != 0) {
        if (redirection_info[command_destination].input_stream == STDIN_FILENO) {
            int pipe_fd[2];
            pipe(pipe_fd);
            redirection_info[0].output_stream = pipe_fd[1];
            if (pipe_stderr)
                redirection_info[0].error_stream = pipe_fd[1];

            redirection_info[command_destination].input_stream = pipe_fd[0];
            redirection_info[command_destination].pipe_write_end = pipe_fd[1];
        } else {
            redirection_info[0].output_stream = redirection_info[command_destination].pipe_write_end;
        }
    }
    commands_group = split_and_trim(usr_input_copy, "|");

    for (int i = 0; i < commands_group.size();) {
        auto left_boundary = std::next(commands_group.begin(), i);
        auto right_boundary = (i + 50 < commands_group.size()) ? std::next(left_boundary, 50) : commands_group.end();
        i += std::distance(left_boundary, right_boundary);

        Vector<String> small_command_group;
        std::copy(left_boundary, right_boundary,
                  std::back_inserter(small_command_group));

        if (right_boundary != commands_group.end()) {
            redirection_info.insert(redirection_info.begin() + 1, Node());
            int pipe_fd[2];
            pipe(pipe_fd);
            redirection_info[0].output_stream = pipe_fd[1];
            redirection_info[1].input_stream = pipe_fd[0];
            redirection_info[1].pipe_write_end = pipe_fd[1];
        }

        signal(SIGCHLD, child_handler);
        // std::cout << "Numberpipe analyze complete" << std::endl;

        size_t pipe_cnt = small_command_group.size() - 1;
        for (size_t j = 0; j < pipe_cnt; ++j) {
            if (pipe(pipes_fd[j]) == -1) {
                throw std::exception();
            }
        }

        int input_stream = -1;
        int output_stream = -1;
        int error_stream = -1;
        for (size_t j = 0; j < small_command_group.size(); ++j) {
            input_stream = (j == 0) ? redirection_info.front().input_stream : pipes_fd[j - 1][0];
            output_stream = (j == small_command_group.size() - 1) ? redirection_info.front().output_stream
                                                                  : pipes_fd[j][1];
            if (pipe_stderr)
                error_stream = (j == small_command_group.size() - 1) ? redirection_info.front().error_stream
                                                                     : pipes_fd[j][1];
            else
                error_stream = STDERR_FILENO;

            pid_t c_pid = fork_process(small_command_group[j], pipe_cnt, input_stream, output_stream, error_stream);
            pid_table.push_back(c_pid);

            // We keep the numbered pipeline alive (i.e. we won't close it ) until it reaches the destination.
            // Note that the children should always closed it's pipeline.
            if (input_stream == redirection_info.front().input_stream && input_stream != STDIN_FILENO) {
                close(redirection_info.front().pipe_write_end);
                close(input_stream);
            };
        }

        // Multi pipe should always closed properly no matter in children or in parent process.
        // We won't need these pipes in the future, definitely.
        for (size_t j = 0; j < pipe_cnt; ++j) {
            close(pipes_fd[j][0]);
            close(pipes_fd[j][1]);
        }

        if (output_stream != STDOUT_FILENO) {
            for (auto first = pid_table.begin(); std::distance(first, pid_table.end()) < pid_table.size()-1; ++first)
                waitpid(*first, nullptr, 0);
            pid_table.clear();
        }
        else {
            for (auto item : pid_table)
                waitpid(item, nullptr, 0);
            pid_table.clear();
        }

        if (right_boundary != commands_group.end())
            redirection_info.pop_front();

    }
}

pid_t
ForkAndExec::fork_process(String command, size_t pipe_cnt, int input_stream, int output_stream, int error_stream) {

    pid_t child_pid;
    while ((child_pid = fork()) < 0) {
        usleep(1000);
    }

    if (child_pid == 0) {
//        std::cout << "[INPUT, OUTPUT, ERROR]: " << input_stream << ' ' << output_stream << ' ' << error_stream
//                  << std::endl;
//        std::cout << "Command name = " << command;


        if (input_stream != STDIN_FILENO) {
            if (dup2(input_stream, STDIN_FILENO))
                throw std::exception();


            if (input_stream == redirection_info[0].input_stream) {
                //If the input_stream comes from a pipe's read_end
                //We close the write_end of that pipe to ensure we can read properly.
                close(redirection_info[0].pipe_write_end);
            }
        }

        if (output_stream != STDOUT_FILENO) {
            if (dup2(output_stream, STDOUT_FILENO) == -1)
                throw std::exception();
        }

        if (error_stream != STDERR_FILENO) {
            if (dup2(error_stream, STDERR_FILENO) == -1)
                throw std::exception();
        }
        for (size_t i = 0; i < pipe_cnt; ++i) {
            close(pipes_fd[i][0]);
            close(pipes_fd[i][1]);
        }

        Vector<String> result = split_and_trim(command, ">");
        if (result.size() > 1) {
            output_stream = open(result[1].c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dup2(output_stream, STDOUT_FILENO) == -1)
                throw std::exception();
            close(output_stream);
            command = result[0];
        }

        Vector<String> space_split_command = split_and_trim(command);
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

String ForkAndExec::get_last_string(String str) {
    size_t i = str.size();
    while (--i) {
        if (str[i] == ' ')
            break;
    }
    if (i != 0) {
        return str.substr(i + 1, str.size() - (i + 1));
    }
    return "";
}

void PrintEnv::execute(const String &usr_input_line) {
    auto string_group = split_and_trim(usr_input_line);
    char *result = getenv(string_group[1].c_str()); // getenv might return a NULL
    if (result)
        std::cout << result << std::endl;
}

void SetEnv::execute(const String &usr_input_line) {
    auto string_group = split_and_trim(usr_input_line);
    setenv(string_group[1].c_str(), string_group[2].c_str(), 1);
}

void Unknown::execute(const String &usr_input_line) {
    auto string_group = split_and_trim(usr_input_line);
    std::cout << "Unknown command:  [" << string_group[0] << "]" << std::endl;
}
