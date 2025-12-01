#ifndef CLIENT_H
#define CLIENT_H

#include <pthread.h>

// Listener thread (implemented in receiver_thread.c)
void *listener(void *arg);

// Connect to server (optional helper)
int connect_to_server(const char *ip, int port);

#endif
