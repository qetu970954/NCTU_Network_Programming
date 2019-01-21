#pragma once

#include <unistd.h>
#include <cstdio>

namespace Pipes {
    static int pipes_fd[15000][2] = {};

    static void open_pipes(unsigned long N) {
        for (unsigned long j = 0; j < N; ++j) {
            if (pipe(pipes_fd[j]) == -1) {
                perror("PIPE ERROR");
            }
        }
    }

    static void close_pipes(unsigned long N) {
        for (size_t j = 0; j < N; ++j) {
            close(Pipes::pipes_fd[j][0]);
            close(Pipes::pipes_fd[j][1]);
        }
    }
};
