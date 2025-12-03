// server.c — Final, beginner-friendly: graceful shutdown + client removal
// Paste this entire file to: ~/CSIT203_Project/server/server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <signal.h>

#include "server.h"
#include "client_handler.h"

#define MAX_CLIENTS 50

pthread_mutex_t user_lock = PTHREAD_MUTEX_INITIALIZER;
User active_users[MAX_CLIENTS];
int user_count = 0;

/* globals used for shutdown */
int server_fd_global = -1;
volatile sig_atomic_t shutdown_requested = 0;

/* Broadcast a null-terminated message to all connected clients (adds newline). */
void broadcast_shutdown_message(const char *msg) {
    if (!msg) return;
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < user_count; ++i) {
        int fd = active_users[i].socket_fd;
        if (fd > 0) {
            char out[2048];
            snprintf(out, sizeof(out), "%s\n", msg);
            send(fd, out, strlen(out), 0);
        }
    }
    pthread_mutex_unlock(&user_lock);
}

/* SIGINT handler: request shutdown and close listening socket to interrupt accept() */
void handle_sigint(int sig) {
    (void)sig;
    shutdown_requested = 1;
    fprintf(stdout, "\n[SERVER] Ctrl+C pressed. Initiating graceful shutdown...\n");
    fflush(stdout);
    if (server_fd_global != -1) {
        close(server_fd_global);
        server_fd_global = -1;
    }
}

int main() {
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    server_fd_global = socket(AF_INET, SOCK_STREAM, 0);
    int server_fd = server_fd_global;
    if (server_fd < 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen error");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    /* install SIGINT handler for Ctrl+C */
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, NULL);

    printf("[SERVER] Running on port 8080...\n");

    while (!shutdown_requested) {
        int new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
        if (new_socket < 0) {
            if (shutdown_requested) break; /* accept interrupted by signal */
            perror("[SERVER] Accept failed");
            continue;
        }

        printf("[SERVER] New connection.\n");

        pthread_t tid;
        int *socket_ptr = malloc(sizeof(int));
        if (!socket_ptr) {
            close(new_socket);
            continue;
        }
        *socket_ptr = new_socket;

        pthread_create(&tid, NULL, handle_client, socket_ptr);
        pthread_detach(tid);
    }

    /* Begin graceful shutdown */
    printf("[SERVER] Broadcasting shutdown message...\n");
    broadcast_shutdown_message("Server shutting down...");

    /* Wait a short moment to let clients receive the message */
    usleep(400000); /* 400 ms */

    printf("[SERVER] Closing all client sockets...\n");

    pthread_mutex_lock(&user_lock);
    /* polite shutdown to help TCP flush */
    for (int i = 0; i < user_count; ++i) {
        int fd = active_users[i].socket_fd;
        if (fd > 0) shutdown(fd, SHUT_WR);
    }
    usleep(100000); /* 100 ms */

    for (int i = 0; i < user_count; ++i) {
        int fd = active_users[i].socket_fd;
        if (fd > 0) close(fd);
        active_users[i].socket_fd = -1;
        active_users[i].username[0] = '\0';
    }
    user_count = 0;
    pthread_mutex_unlock(&user_lock);

    printf("[SERVER] Shutdown complete. Exiting.\n");
    return 0;
}
