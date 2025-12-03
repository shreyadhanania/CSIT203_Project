#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>

#define MAX_CLIENTS 50

typedef struct {
    char username[50];
    int socket_fd;
} User;

extern User active_users[];
extern int user_count;

#endif
