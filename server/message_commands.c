#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "message_commands.h"
#include "database.h"
#include "server.h"   // for User, active_users, user_count
// (server.h should declare: extern User active_users[]; extern int user_count;)
 
extern pthread_mutex_t user_lock;

// Helper: find username for this socket
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

// getmessages <user>
void handle_getmessages(int client_socket, char *raw_command) {
    char current_user[50];

    if (!get_username_for_socket(client_socket, current_user)) {
        const char *err = "ERROR: Unknown user.\n";
        write(client_socket, err, strlen(err));
        return;
    }

    // raw_command looks like: "getmessages bob\n"
    // Simple parse with strtok:
    char *cmd = strtok(raw_command, " ");       // "getmessages"
    char *other = strtok(NULL, " \n");          // "bob"

    if (!other) {
        const char *usage = "Usage: getmessages <user>\n";
        write(client_socket, usage, strlen(usage));
        return;
    }

    // Get chat history between current_user and other
    db_get_messages(current_user, other, client_socket);
}

// deletemessages <user>
void handle_deletemessages(int client_socket, char *raw_command) {
    char current_user[50];

    if (!get_username_for_socket(client_socket, current_user)) {
        const char *err = "ERROR: Unknown user.\n";
        write(client_socket, err, strlen(err));
        return;
    }

    char *cmd = strtok(raw_command, " ");      // "deletemessages"
    char *other = strtok(NULL, " \n");         // "bob"

    if (!other) {
        const char *usage = "Usage: deletemessages <user>\n";
        write(client_socket, usage, strlen(usage));
        return;
    }

    db_delete_messages(current_user, other);

    const char *ok = "Messages deleted successfully.\n";
    write(client_socket, ok, strlen(ok));
}
