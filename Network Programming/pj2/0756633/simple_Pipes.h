#ifndef NETWORKPROGRAMMING_PIPES_H
#define NETWORKPROGRAMMING_PIPES_H

#include <unistd.h>
#include <zconf.h>
#include <iostream>

class Pipes {
public:
    static void open_pipes(unsigned long N) {
        for (unsigned long j = 0; j < N; ++j) {
            if (pipe(pipes_fd[j]) == -1) {
                std::cerr << "PIPE ERROR";
                throw std::exception();
            }
        }
    }

    static void close_pipes(unsigned long N) {
        for (size_t j = 0; j < N; ++j) {
            close(Pipes::pipes_fd[j][0]);
            close(Pipes::pipes_fd[j][1]);
        }
    }

    static int pipes_fd[15000][2];
};

#endif //NETWORKPROGRAMMING_PIPES_H
