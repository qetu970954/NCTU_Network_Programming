#include <netinet/in.h>
#include "simple_Shell.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
#pragma clang diagnostic ignored "-Wconversion"
#define BUFFER_SIZE 1024


int main(int argc, char *argv[]) {
    if (argc < 2) printf("Usage: %s [port]\n", argv[0]);

    int port = atoi(argv[1]);

    int server_fd;
    sockaddr_in server{}, client{};
    char buf[BUFFER_SIZE];

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0)
        perror("Could not create socket\n");

    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    int opt_val = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

    if (bind(server_fd, (struct sockaddr *) &server, sizeof(server)) < 0)
        perror("Could not bind socket\n");

    if (listen(server_fd, 128) < 0)
        perror("Could not listen on socket\n");

    printf("Server is listening on %d\n", port);

    while (true) {
        socklen_t client_len = sizeof(client);
        int client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
        if (client_fd < 0) perror("Could not establish new connection\n");
        pid_t pid = fork();
        if( pid == 0 ){
            dup2(client_fd, STDIN_FILENO);
            dup2(client_fd, STDOUT_FILENO);
            dup2(client_fd, STDERR_FILENO);
            Shell shell;
            shell.main_loop();
            close(client_fd);
            exit(EXIT_SUCCESS);
        }else{
            wait(nullptr);
            close(client_fd);
        }
    }
}

#pragma clang diagnostic pop