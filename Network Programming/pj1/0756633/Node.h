#ifndef NETWORKPROGRAMMING_NODE_H
#define NETWORKPROGRAMMING_NODE_H


struct Node {
    int input_stream = STDIN_FILENO;
    int output_stream = STDOUT_FILENO;
    int error_stream = STDERR_FILENO;
    int pipe_write_end = -1;

    bool is_default() {
        return input_stream == STDIN_FILENO && output_stream == STDOUT_FILENO;
    }
};


#endif //NETWORKPROGRAMMING_NODE_H
