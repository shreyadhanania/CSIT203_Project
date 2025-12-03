#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "server.h"
#include "user_list.h"

extern User active_users[];
extern int user_count;
extern pthread_mutex_t user_lock;

void get_userlist(char *buffer, size_t size) {
    pthread_mutex_lock(&user_lock);

    buffer[0] = '\0';
    snprintf(buffer, size, "Active Users:\n");

    for (int i = 0; i < user_count; i++) {
        strncat(buffer, active_users[i].username, size - strlen(buffer) - 1);
        strncat(buffer, "\n", size - strlen(buffer) - 1);
    }

    pthread_mutex_unlock(&user_lock);
}
