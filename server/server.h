#ifndef SERVER_H
#define SERVER_H

typedef struct {
    char username[50];
    int socket_fd;
} User;

extern User active_users[];
extern int user_count;

#endif
