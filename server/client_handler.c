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

        // Basic parser (team will expand this): Shreya
        if (strncmp(buffer, "sendmessage", 11) == 0) {
            // Format from client: "sendmessage <user> <message>"

            // Extract sender username
            char sender[50];
            int found = 0;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (active_users[i].socket_fd == client_socket) {
                    strcpy(sender, active_users[i].username);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (!found) {
                send(client_socket, "ERROR: Unknown user.\n", 22, 0);
                return NULL;
            }

            // Parse command
            char *cmd = strtok(buffer, " ");     // "sendmessage"
            char *receiver = strtok(NULL, " ");  // <user>
            char *message = strtok(NULL, "");    // <message text>

            if (!receiver || !message) {
                send(client_socket, "Usage: sendmessage <user> <message>\n", 37, 0);
                return NULL;
            }

            // SEARCH if receiver exists and is online
            int receiver_socket = -1;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(active_users[i].username, receiver) == 0) {
                    receiver_socket = active_users[i].socket_fd;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            // IF RECEIVER IS ONLINE
            if (receiver_socket != -1) {
                char deliver_msg[1200];
                sprintf(deliver_msg, "Message from %s: %s\n", sender, message);

                send(receiver_socket, deliver_msg, strlen(deliver_msg), 0);

                send(client_socket, "Message delivered.\n", 20, 0);
            } 
            else {
                // Receiver offline → (optional) store for later
                send(client_socket, "User offline. Message not delivered.\n", 38, 0);
            }

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
