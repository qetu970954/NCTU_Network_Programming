#include "multi_proc_Command.h"


Deque<Node> Command::redirection_info(1500);

void ForkAndExec::execute(const String &line) {
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

pid_t ForkAndExec::fork_process(Vector<String> command_and_its_arguments, size_t pipe_cnt, int input_stream, int output_stream, int error_stream) {
    pid_t child_pid;
    while ((child_pid = fork()) < 0) {
        usleep(1000);
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


        char *arg[command_and_its_arguments.size() + 1];
        for (auto i = 0; i < command_and_its_arguments.size(); ++i)
            arg[i] = const_cast<char *>(command_and_its_arguments[i].c_str());
        arg[command_and_its_arguments.size()] = nullptr;
        execvp(arg[0], arg);

        std::cerr << "Unknown command: [" << arg[0] << "]." << std::endl;
        std::cerr.flush();
        exit(EXIT_FAILURE);
    }
    return child_pid;
}

bool ForkAndExec::parse_input_from_user_pipe(Vector<String> *commands, int *input_stream, Vector<int> *filefd_to_close, String line) {
    // If parsing goes wrong, it will return false.
    using namespace std;
    for (auto iter = commands->begin(); iter != commands->end();) {
        if (iter->front() == '<' && iter->size() >= 2) {
            Client sender = SharedMemory::get_instance().get_client_copy_at(stoi(iter->substr(1)));
            Client me = SharedMemory::get_instance().get_me();

            if (sender.id == 0) {
                String msg = "*** Error: user #" + iter->substr(1) + " does not exist yet. ***\n"; //sender not exist
                cout << msg << endl;
                return false;
            }

            int result;
            try {
                result = PostOffice::get_instance().erase(sender.id, me.id);
            } catch (exception &e) {
                cout << e.what() << endl;
                return false;
            }

            if (result == -1) {
                perror("Error opening file");
                return false;
            }

            *input_stream = result;
            filefd_to_close->push_back(result);
            String msg = String("*** ") + me.name + " (#" + to_string(me.id) + ") just received from " +
                         sender.name + " (#" + to_string(sender.id) + ") by '" + line + "' ***\n";
            SharedMemory::get_instance().broadcast(msg);
            commands->erase(iter);
            break;
        } else
            ++iter;
    }

    return true;
}

bool ForkAndExec::parse_output_to_user_pipe(Vector<String> *commands, int *output_stream, Vector<int> *filefd_to_close, const String &line) {
    // If parsing goes wrong, it will return false.
    using namespace std;
    for (auto iter = commands->begin(); iter != commands->end();) {
        if (iter->front() == '>' && iter->size() >= 2) {
            Client me = SharedMemory::get_instance().get_me();
            Client receiver = SharedMemory::get_instance().get_client_copy_at(stoi(iter->substr(1)));
            if (receiver.id == 0) {
                String msg = "*** Error: user #" + iter->substr(1) + " does not exist yet. ***";
                std::cout << msg << std::endl;
                return false;
            }

            int result;
            try {
                result = PostOffice::get_instance().insert(me.id, receiver.id);
            } catch (std::exception &e) {
                std::cout << e.what() << std::endl;
                return false;
            }

            if (result == -1) {
                perror("Error creating file");
                return false;
            }

            *output_stream = result;
            filefd_to_close->push_back(result);
            String msg = String("*** ") + me.name + " (#" + to_string(me.id) + ") just piped '" + line +
                         "' to " + receiver.name + " (#" + to_string(receiver.id) + ") ***\n";
            SharedMemory::get_instance().broadcast(msg);
            commands->erase(iter);
            break;
        }
        else
            ++iter;
    }
    return true;
}

void ForkAndExec::parse_output_to_file(Vector<String> *commands, int *output_stream) {
    if (commands->size() > 2) {
        if (commands->at(commands->size() - 2) == ">") {
            *output_stream = open(commands->back().c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
            commands->pop_back();
            commands->pop_back();
        }
    }
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
