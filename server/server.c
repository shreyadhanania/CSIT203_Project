#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#include "server.h"
#include "client_handler.h"

#define MAX_CLIENTS 50

pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;
User active_users[MAX_CLIENTS];
int user_count = 0;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1, addrlen = sizeof(address);

    // Create server socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow reuse of port
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("Listen error");
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Running on port 8080...\n");

    while (1) {
        new_socket = accept(server_fd, (struct sockaddr*)&address, (socklen_t*)&addrlen);

        if (new_socket < 0) {
            perror("Accept failed");
            continue;
        }

        printf("[SERVER] New connection.\n");

        pthread_t tid;
        int *socket_ptr = malloc(sizeof(int));
        *socket_ptr = new_socket;

        pthread_create(&tid, NULL, handle_client, socket_ptr);
        pthread_detach(tid);
    }

    return 0;
}
