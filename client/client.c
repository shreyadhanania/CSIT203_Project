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

    //SEND USERNAME USING "connect <username>" COMMAND

    char username[50];   //Buffer to hold the username input
    printf("Enter username: ");    //Prompt the user to enter a username
    fgets(username, sizeof(username), stdin);   //Read the username from standard input
    username[strcspn(username, "\n")] = 0;    //Remove the newline character from the input

    char connect_cmd[100];    //Buffer to hold the formatted connect command
    sprintf(connect_cmd, "connect %s\n", username);    //Format the connect command with the username

    send(sock, connect_cmd, strlen(connect_cmd), 0);    //Send the connect command to the server


    pthread_t tid;
    pthread_create(&tid, NULL, listener, &sock);

    char buffer[1024];

    while (1) {
        printf("> ");
        fflush(stdout);

        if (!fgets(buffer, sizeof(buffer), stdin))
            break;

        send(sock, buffer, strlen(buffer), 0);

        // EXIT handling
        if (strncmp(buffer, "exit", 4) == 0) {
            // Wait for server's bye message
            char recvbuf[1024];
            int n = recv(sock, recvbuf, sizeof(recvbuf) - 1, 0);

            if (n > 0) {
                recvbuf[n] = '\0';
                printf("%s", recvbuf);   // <-- prints “Byeeee <username>! …”
            }

            printf("Connection closed.\n");
            break;
        }

    }

    close(sock);
    return 0;
}
