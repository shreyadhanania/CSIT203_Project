#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>     // free()
#include <unistd.h>     // close(), read(), write()
#include <sys/socket.h> // send(), recv()
#include <string.h>     // strlen(), strcmp()

#include "client_handler.h"
#include "server.h"

extern pthread_mutex_t user_lock;

void *handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);

    char buffer[1024];

    // TODO (Aditi): Perform username registration
    send(client_socket, "Enter username: ", 16, 0);
    int valread = read(client_socket, buffer, 1024);
    buffer[valread - 1] = '\0';

    pthread_mutex_lock(&user_lock);
    strcpy(active_users[user_count].username, buffer);
    active_users[user_count].socket_fd = client_socket;
    user_count++;
    pthread_mutex_unlock(&user_lock);

    printf("[SERVER] User %s connected.\n", buffer);

    // Main command loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        valread = read(client_socket, buffer, 1024);
        if (valread <= 0) break;

        buffer[valread] = '\0';

        printf("[CLIENT CMD] %s\n", buffer);

        // Basic parser (team will expand this)
        if (strncmp(buffer, "sendmessage", 11) == 0) {
            // TODO Shreya
        }
        else if (strncmp(buffer, "getmessages", 11) == 0) {
            // TODO Varnika
        }
        else if (strncmp(buffer, "deletemessages", 14) == 0) {
            // TODO Varnika
        }
        else if (strncmp(buffer, "getuserlist", 11) == 0) {
            // TODO Navreet
        }
        else if (strncmp(buffer, "exit", 4) == 0) {
            // TODO Flora: graceful client shutdown
            break;
        }
    }

    close(client_socket);
    return NULL;
}
