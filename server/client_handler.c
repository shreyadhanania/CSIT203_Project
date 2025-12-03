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

    char buffer[2048];

    // ---- CONNECTION MANAGEMENT: "connect <username>" ----
    int valread = read(client_socket, buffer, sizeof(buffer) - 1);
    if (valread <= 0) {
        close(client_socket);
        return NULL;
    }

    buffer[valread] = '\0';

    char command[32], username[50];

    // Expect: connect <username>
    if (sscanf(buffer, "%31s %49s", command, username) != 2) {
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

    // Trim newline/whitespace from username
    trim_newline(username);

    // Check username uniqueness and capacity
    pthread_mutex_lock(&user_lock);

    if (user_count >= MAX_CLIENTS) {
        pthread_mutex_unlock(&user_lock);
        const char *err = "ERROR: Server full. Try later.\n";
        send(client_socket, err, strlen(err), 0);
        close(client_socket);
        return NULL;
    }

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
    strncpy(active_users[user_count].username, username, sizeof(active_users[user_count].username)-1);
    active_users[user_count].username[sizeof(active_users[user_count].username)-1] = '\0';
    active_users[user_count].socket_fd = client_socket;
    user_count++;

    pthread_mutex_unlock(&user_lock);

    // Confirm registration
    char success_msg[128];
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

        // Remove trailing newline for easier parsing
        trim_newline(buffer);

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

            // 2) parse command: separate receiver and message
            // We want to preserve spaces in message, so find first two tokens
            char *space = strchr(buffer, ' ');
            if (!space) {
                const char *usage = "Usage: sendmessage <user> <message>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }
            char *after_cmd = space + 1;
            char receiver[50];
            // get receiver token
            if (sscanf(after_cmd, "%49s", receiver) != 1) {
                const char *usage = "Usage: sendmessage <user> <message>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }
            // message begins after receiver token
            char *msg_start = strchr(after_cmd, ' ');
            if (!msg_start) {
                const char *usage = "Usage: sendmessage <user> <message>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }
            // skip the space after receiver
            msg_start = msg_start + 1;

            // store in DB (db functions handle their own locking)
            db_store_message(sender, receiver, msg_start);

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
                // include timestamp for forwarded message
                char timestamp[64];
                current_timestamp(timestamp, sizeof(timestamp));

                char deliver_msg[1600];
                snprintf(deliver_msg, sizeof(deliver_msg),
                         "[%s] Message from %s: %s\n", timestamp, sender, msg_start);

                send(receiver_socket, deliver_msg, strlen(deliver_msg), 0);
                const char *ok = "Message delivered.\n";
                send(client_socket, ok, strlen(ok), 0);
            } else {
                const char *offline =
                    "User offline. Message stored for later retrieval.\n";
                send(client_socket, offline, strlen(offline), 0);
            }
        }

        // GETMESSAGES -------------------------------------------------
        else if (strncmp(buffer, "getmessages", 11) == 0) {
            char current_user[50];

            // 1) Identify which user is asking
            if (!get_username_for_socket(client_socket, current_user)) {
                const char *err = "ERROR: Unknown user.\n";
                send(client_socket, err, strlen(err), 0);
                continue;
            }

            // 2) Parse: getmessages <other_user>
            char cmd[64], other[50];
            if (sscanf(buffer, "%63s %49s", cmd, other) != 2) {
                const char *usage = "Usage: getmessages <user>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }

            // 3) Check if ANY HISTORY EXISTS (offline or online)
            int exists = db_user_exists_in_history(current_user, other);

            if (!exists) {
                const char *no_hist = "No message history with this user.\n";
                send(client_socket, no_hist, strlen(no_hist), 0);
                continue;
            }

            // 4) Display all messages (from DB)
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
            char cmd[64], other[50];
            if (sscanf(buffer, "%63s %49s", cmd, other) != 2) {
                const char *usage = "Usage: deletemessages <user>\n";
                send(client_socket, usage, strlen(usage), 0);
                continue;
            }

            // 3. Allow messages even if user is offline
            int exists = db_user_exists_in_history(current_user, other);

            if (!exists) {
                const char *no_hist = "No message history with this user.\n";
                send(client_socket, no_hist, strlen(no_hist), 0);
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

            char listbuf[1024];
            int offset = 0;

            offset += snprintf(listbuf + offset,
                               sizeof(listbuf) - offset,
                               "Active Users:\n");

            for (int i = 0; i < user_count; i++) {
                offset += snprintf(listbuf + offset,
                                   sizeof(listbuf) - offset,
                                   "%s\n", active_users[i].username);
                if (offset >= (int)sizeof(listbuf) - 100) break;
            }

            pthread_mutex_unlock(&user_lock);

            send(client_socket, listbuf, strlen(listbuf), 0);
        }

        // EXIT --------------------------------------------------------
        else if (strncmp(buffer, "exit", 4) == 0) {

            char username[50];
            int found = get_username_for_socket(client_socket, username);

            if (found) {
                // 1. Send bye message to the exiting client
                char bye_msg[128];
                snprintf(bye_msg, sizeof(bye_msg),
                        "Byeeee %s! You have been disconnected.\n", username);
                send(client_socket, bye_msg, strlen(bye_msg), 0);

                printf("[SERVER] User requested exit: %s\n", username);

                // 2. Broadcast to other online users
                pthread_mutex_lock(&user_lock);
                for (int i = 0; i < user_count; i++) {
                    int other_socket = active_users[i].socket_fd;

                    if (other_socket != client_socket) {
                        char notify_msg[128];
                        snprintf(notify_msg, sizeof(notify_msg),
                                "%s has gone offline.\n", username);

                        send(other_socket, notify_msg, strlen(notify_msg), 0);
                    }
                }
                pthread_mutex_unlock(&user_lock);
            }

            // 3. Exit the loop → triggers cleanup at bottom
            break;
        } else {
            const char *unknown = "Unknown command. Available: sendmessage, getmessages, deletemessages, getuserlist, exit\n";
            send(client_socket, unknown, strlen(unknown), 0);
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
