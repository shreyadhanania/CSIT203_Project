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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: ./client <server_ip>\n");
        return 1;
    }

    int sock = connect_to_server(argv[1], 8080);

    char username[50];
    printf("Enter username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = 0;

    char connect_cmd[100];
    sprintf(connect_cmd, "connect %s\n", username);
    send(sock, connect_cmd, strlen(connect_cmd), 0);

    // SAFE THREAD ARG PASSING
    int *sock_ptr = malloc(sizeof(int));
    *sock_ptr = sock;

    pthread_t tid;
    pthread_create(&tid, NULL, listener, sock_ptr);

    char buffer[1024];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin))
            break;

        buffer[strcspn(buffer, "\n")] = 0; // remove newline
        char cleanbuf[1024];
        sprintf(cleanbuf, "%s\n", buffer);

        send(sock, cleanbuf, strlen(cleanbuf), 0);

        if (strncmp(buffer, "exit", 4) == 0) {
            char recvbuf[1024];
            int n = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

            if (n > 0) {
                recvbuf[n] = '\0';
                printf("%s", recvbuf);
            }

            shutdown(sock, SHUT_RDWR);
            printf("Connection closed.\n");
            break;
        }
    }

    close(sock);
    return 0;
}
