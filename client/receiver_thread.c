#include <stdio.h>
#include <unistd.h>
#include "client.h"

void *listener(void *arg) {
    int sock = *(int*)arg;
    char buffer[1024];

    while (1) {
        int bytes = read(sock, buffer, sizeof(buffer)-1);
        if (bytes <= 0)
            break;

        buffer[bytes] = '\0';
        printf("\n%s\n> ", buffer);
        fflush(stdout);
    }

    return NULL;
}

