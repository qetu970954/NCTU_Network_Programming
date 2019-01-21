#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/shm.h>
#include "multi_proc_Shell.h"


static void sig_handler(int sig);

int build_passive_TCP(int port, int queue_length);


int main(int argc, char *argv[]) {
    if (argc != 2) {
        perror("Usage: ./server <port>");
        return -1;
    }
    std::cout << "Wait to accept" << std::endl;
    int master, client_socket, port = atoi(argv[1]);
    socklen_t client_length = sizeof(sockaddr_in);
    pid_t child_pid;
    sockaddr_in client_address;

    signal(SIGCHLD, sig_handler);
    signal(SIGINT, sig_handler);
    signal(SIGQUIT, sig_handler);
    signal(SIGTERM, sig_handler);

    SharedMemory::get_instance().create_clients();
    // build a TCP connection
    if ((master = build_passive_TCP(port, 0)) < 0) {
        perror("server error: passiveTCP failed\n");
        return -1;
    }

    while (true) {
        client_socket = accept(master, (struct sockaddr *) &client_address, &client_length);
        if (client_socket < 0) {
            if (errno == EINTR)
                continue;
            perror("Server error :");
            return -1;
        }
        child_pid = fork();
        if (child_pid < 0) {
            perror("Server Error :");
            return -1;
        } else if (child_pid == 0) {
            dup2(client_socket, STDIN_FILENO);
            dup2(client_socket, STDOUT_FILENO);
            dup2(client_socket, STDERR_FILENO);
            close(master);
            close(client_socket);

            Shell shell(&client_address);
            shell.main_loop();
            shell.~Shell();
            std::cout.flush();
            std::cerr.flush();
            exit(EXIT_SUCCESS);
        } else { close(client_socket); };
    }
}

void sig_handler(int sig) {
    if (sig == SIGCHLD) {
        while (waitpid(-1, NULL, WNOHANG) > 0);
    }
    if (sig == SIGINT || sig == SIGQUIT || sig == SIGTERM) {
        // free shared memory
        if (!SharedMemory::get_instance().delete_shared_memory()) {
            std::cerr << "Error : Nothing is deleted." << std::endl;
        }
        exit(0);
    }
    signal(sig, sig_handler);
}

int build_passive_TCP(int port, int queue_length) {
    int socket_fd;
    sockaddr_in server_address{};

    /* open a TCP socket */
    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        fputs("server error: cannot open socket\n", stderr);
        return -1;
    }

    int option = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);

    /* set up server socket addr */
    bzero(&server_address, sizeof(server_address));
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY);
    server_address.sin_port = htons(port);

    /* bind to server address */
    if (bind(socket_fd, (const struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
        fputs("server error: cannot bind local address\n", stderr);
        return -1;
    }

    /* listen for requests */
    if (listen(socket_fd, queue_length) < 0) {
        fputs("server error: listen failed\n", stderr);
        return -1;
    }

    return socket_fd;
}
