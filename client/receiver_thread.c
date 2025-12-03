#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "client.h"

void *listener(void *arg) {
    int sock = *(int*)arg;
    char buffer[1024];

    while (1) {
        ssize_t bytes = read(sock, buffer, sizeof(buffer) - 1);

        if (bytes <= 0) {
            printf("\n[INFO] Server closed connection\n");
            printf("Connection closed.\n");
            return NULL;
        }

        buffer[bytes] = '\0';

        // Detect server shutdown broadcast
        if (strstr(buffer, "Server shutting down") != NULL) {

            printf("\n%s\n", buffer);   // print broadcast

            printf("[INFO] Server closed connection\n");
            printf("Connection closed.\n");

            close(sock);
            return NULL;
        }

        // Normal incoming message
        printf("\n%s\n> ", buffer);
        fflush(stdout);
    }

    return NULL;
}
