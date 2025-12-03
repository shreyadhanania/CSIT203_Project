// client_handler.c

#include <stdio.h>
#include <stdlib.h>     // malloc, free
#include <string.h>     // strlen, strcmp, strcpy, memset
#include <unistd.h>     // close, read, write
#include <pthread.h>
#include <sys/socket.h> // send, recv

#include "client_handler.h"
#include "server.h"
#include "database.h"
#include "utils.h"

extern pthread_mutex_t user_lock;
extern User active_users[];
extern int user_count;

// Helper: find username for a given client socket.
// Returns 1 if found (and copies into out_username), 0 otherwise.
static int get_username_for_socket(int client_socket, char *out_username) {
    int found = 0;

    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < user_count; i++) {
        if (active_users[i].socket_fd == client_socket) {
            strcpy(out_username, active_users[i].username);
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&user_lock);

    return found;
}

void *handle_client(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);

    char buffer[1024];

    // ---- CONNECTION MANAGEMENT: "connect <username>" ----
    int valread = read(client_socket, buffer, sizeof(buffer) - 1);
    if (valread <= 0) {
        close(client_socket);
        return NULL;
    }

    buffer[valread] = '\0';

    char command[20], username[50];

    // Expect: connect <username>
    if (sscanf(buffer, "%19s %49s", command, username) != 2) {
        const char *err = "ERROR: Use format: connect <username>\n";
        send(client_socket, err, strlen(err), 0);
        close(client_socket);
        return NULL;
    }

    if (strcmp(command, "connect") != 0) {
        const char *err = "ERROR: Use format: connect <username>\n";
        send(client_socket, err, strlen(err), 0);
        close(client_socket);
        return NULL;
    }

    // Check username uniqueness
    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(active_users[i].username, username) == 0) {
            pthread_mutex_unlock(&user_lock);

            printf("[SERVER] Username already taken: %s\n", username);

            const char *err = "ERROR: Username already taken!\n";
            send(client_socket, err, strlen(err), 0);

            close(client_socket);
            return NULL;
        }
    }

    // Register new user
    strcpy(active_users[user_count].username, username);
    active_users[user_count].socket_fd = client_socket;
    user_count++;

    pthread_mutex_unlock(&user_lock);

    // Confirm registration
    char success_msg[100];
    snprintf(success_msg, sizeof(success_msg),
             "Connected successfully as %s\n", username);
    send(client_socket, success_msg, strlen(success_msg), 0);

    printf("[SERVER] Registered new user: %s\n", username);

    // ---------------- MAIN COMMAND LOOP ----------------
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        valread = read(client_socket, buffer, sizeof(buffer) - 1);
        if (valread <= 0)
            break;

        buffer[valread] = '\0';

        printf("[CLIENT CMD] %s\n", buffer);

        // SENDMESSAGE -------------------------------------------------
        if (strncmp(buffer, "sendmessage", 11) == 0) {
            // Format: "sendmessage <user> <message>"

            // 1) find sender username
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
                const char *err = "ERROR: Unknown user.\n";
                send(client_socket, err, strlen(err), 0);
                continue;
            }

            // 2) parse command
            char *cmd     = strtok(buffer, " ");  // "sendmessage"
            char *receiver = strtok(NULL, " ");   // <user>
            char *message  = strtok(NULL, "");    // <message text>

            (void)cmd; // unused variable warning silencer

            if (!receiver || !message) {
                const char *usage =
                    "Usage: sendmessage <user> <message>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }

            // store in DB
            db_store_message(sender, receiver, message);

            // 3) find receiver socket if online
            int receiver_socket = -1;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(active_users[i].username, receiver) == 0) {
                    receiver_socket = active_users[i].socket_fd;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (receiver_socket != -1) {
                char deliver_msg[1200];
                snprintf(deliver_msg, sizeof(deliver_msg),
                         "Message from %s: %s\n", sender, message);

                send(receiver_socket, deliver_msg, strlen(deliver_msg), 0);
                const char *ok = "Message delivered.\n";
                send(client_socket, ok, strlen(ok), 0);
            } else {
                const char *offline =
                    "User offline. Message not delivered.\n";
                send(client_socket, offline, strlen(offline), 0);
            }
        }

        // GETMESSAGES -------------------------------------------------
        else if (strncmp(buffer, "getmessages", 11) == 0) {
            char current_user[50];

            // 1) who is asking?
            if (!get_username_for_socket(client_socket, current_user)) {
                const char *err = "ERROR: Unknown user.\n";
                send(client_socket, err, strlen(err), 0);
                continue;
            }

            // 2) parse "getmessages <other_user>"
            char *cmd   = strtok(buffer, " ");     // "getmessages"
            char *other = strtok(NULL, " \n");     // <other_user>
            (void)cmd;

            if (!other) {
                const char *usage =
                    "Usage: getmessages <user>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }

            // 3) check if target user exists
            int exists = 0;
            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(active_users[i].username, other) == 0) {
                    exists = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (!exists) {
                const char *no_user = "Client does not exist.\n";
                send(client_socket, no_user, strlen(no_user), 0);
                continue;
            }

            // 4) show all messages between current_user and other
            db_get_messages(current_user, other, client_socket);
        }

        // DELETEMESSAGES ----------------------------------------------
        else if (strncmp(buffer, "deletemessages", 14) == 0) {
            char current_user[50];

            // 1) who is asking?
            if (!get_username_for_socket(client_socket, current_user)) {
                const char *err = "ERROR: Unknown user.\n";
                send(client_socket, err, strlen(err), 0);
                continue;
            }

            // 2) parse "deletemessages <other_user>"
            char *cmd   = strtok(buffer, " ");     // "deletemessages"
            char *other = strtok(NULL, " \n");     // <other_user>
            (void)cmd;

            if (!other) {
                const char *usage =
                    "Usage: deletemessages <user>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }

            // 3) check if target user exists
            int exists = 0;
            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(active_users[i].username, other) == 0) {
                    exists = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (!exists) {
                const char *no_user = "Client does not exist.\n";
                send(client_socket, no_user, strlen(no_user), 0);
                continue;
            }

            // 4) delete all stored messages between them
            db_delete_messages(current_user, other);

            const char *ok = "Messages deleted successfully.\n";
            send(client_socket, ok, strlen(ok), 0);
        }

        // GETUSERLIST -------------------------------------------------
        else if (strncmp(buffer, "getuserlist", 11) == 0) {
            pthread_mutex_lock(&user_lock);

            char listbuf[512];
            int offset = 0;

            offset += snprintf(listbuf + offset,
                               sizeof(listbuf) - offset,
                               "Active Users:\n");

            for (int i = 0; i < user_count; i++) {
                offset += snprintf(listbuf + offset,
                                   sizeof(listbuf) - offset,
                                   "%s\n", active_users[i].username);
            }

            pthread_mutex_unlock(&user_lock);

            send(client_socket, listbuf, strlen(listbuf), 0);
        }

        // EXIT --------------------------------------------------------
        else if (strncmp(buffer, "exit", 4) == 0) {
            // Flora: graceful client shutdown (basic version)
            break;
        }
    }

    // ------------- USER CLEANUP AND DISCONNECT ----------------
    int user_index = -1;

    pthread_mutex_lock(&user_lock);
    for (int i = 0; i < user_count; i++) {
        if (active_users[i].socket_fd == client_socket) {
            user_index = i;
            break;
        }
    }

    if (user_index != -1) {
        printf("[SERVER] Disconnecting user: %s\n",
               active_users[user_index].username);

        for (int i = user_index; i < user_count - 1; i++) {
            active_users[i] = active_users[i + 1];
        }
        user_count--;
    }
    pthread_mutex_unlock(&user_lock);

    close(client_socket);
    return NULL;
}
