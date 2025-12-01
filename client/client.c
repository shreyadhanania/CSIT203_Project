#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "client.h"

int connect_to_server(const char *ip, int port) {
    int sock;
    struct sockaddr_in serv_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket error");
        exit(1);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address\n");
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    return sock;
}

int main() {
    int sock = connect_to_server("127.0.0.1", 8080);

    pthread_t tid;
    pthread_create(&tid, NULL, listener, &sock);

    char buffer[1024];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin))
            break;

        send(sock, buffer, strlen(buffer), 0);

        if (strncmp(buffer, "exit", 4) == 0)
            break;
    }

    close(sock);
    return 0;
}
