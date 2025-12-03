#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>     // free()
#include <unistd.h>     // close(), read(), write()
#include <sys/socket.h> // send(), recv()
#include <string.h>     // strlen(), strcmp()

#include "client_handler.h"
#include "server.h"

extern pthread_mutex_t user_lock;

void *handle_client(void *arg) {
    int client_socket = *(int*)arg;
    free(arg);


// CONNECTION MANAGEMENT: handle the "connect <username>" command

char buffer[1024]; //Declare a character array (buffer) to store data read from the client (up to 1023 bytes)
int valread = read(client_socket, buffer, sizeof(buffer) - 1); //Read data from the client's socket into the buffer; store the number of bytes read in valread

if (valread <= 0) {  //Check if read failed (returns -1) or the client closed the connection (returns 0)
    close(client_socket);   //Close the client socket to free up resources
    return NULL; //Exit the current thread/function (since the connection is lost)
}

buffer[valread] = '\0';  //Null-terminate the buffer to make it a valid string

char command[20], username[50]; //Declare variables to hold the extracted command (e.g., "connect") and the username

// Expect: connect <username>
if (sscanf(buffer, "%s %s", command, username) != 2) {   //Use sscanf to parse the buffer, expecting to successfully extract two strings (command and username)
    char *err = "ERROR: Use format: connect <username>\n";  //Error message for incorrect format
    send(client_socket, err, strlen(err), 0);  //Send the error message back to the client
    close(client_socket);   //Close the client socket
    return NULL;   //Exit the current thread/function
}

if (strcmp(command, "connect") != 0) {    //Check if the extracted command is not "connect"
    char *err = "ERROR: Use format: connect <username>\n";  //Error message for incorrect command 
    send(client_socket, err, strlen(err), 0);  //Send the error message back to the client
    close(client_socket);  //Close the client socket
    return NULL;  //Exit the current thread/function
}

// Check username uniqueness
pthread_mutex_lock(&user_lock);   //Acquire the mutex lock to ensure thread-safe access to the shared active_users list

for (int i = 0; i < user_count; i++) {   //Iterate through the list of currently active users
    if (strcmp(active_users[i].username, username) == 0) 
    {
    pthread_mutex_unlock(&user_lock);

    printf("[SERVER] Username already taken: %s\n", username);   // <--- ADD THIS

    char *err = "ERROR: Username already taken!\n";
    send(client_socket, err, strlen(err), 0);

    close(client_socket);
    return NULL;
    }
}

// Register new user
strcpy(active_users[user_count].username, username);   //Copy the new username into the active_users list
active_users[user_count].socket_fd = client_socket;   //Store the client's socket file descriptor
user_count++;   //Increment the count of active users

pthread_mutex_unlock(&user_lock);  //Release the mutex lock after updating the shared data

// Confirm registration
char success_msg[100];  //Prepare a success message
sprintf(success_msg, "Connected successfully as %s\n", username);  //Format the success message with the username
send(client_socket, success_msg, strlen(success_msg), 0);  //Send the success message back to the client

printf("[SERVER] Registered new user: %s\n", username);  //Print a confirmation message to the server console


    // Main command loop
    while (1) {
        memset(buffer, 0, sizeof(buffer));
        valread = read(client_socket, buffer, 1024);
        if (valread <= 0) break;

        buffer[valread] = '\0';

        printf("[CLIENT CMD] %s\n", buffer);

        // Basic parser (team will expand this): Shreya
        if (strncmp(buffer, "sendmessage", 11) == 0) {
            // Format from client: "sendmessage <user> <message>"

            // Extract sender username
            char sender[50];
            int found = 0;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (active_users[i].socket_fd == client_socket) {
                    strcpy(sender, active_users[i].username);
                    found = 1;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            if (!found) {
                send(client_socket, "ERROR: Unknown user.\n", 22, 0);
                return NULL;
            }

            // Parse command
            char *cmd = strtok(buffer, " ");     // "sendmessage"
            char *receiver = strtok(NULL, " ");  // <user>
            char *message = strtok(NULL, "");    // <message text>

            if (!receiver || !message) {
                send(client_socket, "Usage: sendmessage <user> <message>\n", 37, 0);
                return NULL;
            }

            // SEARCH if receiver exists and is online
            int receiver_socket = -1;

            pthread_mutex_lock(&user_lock);
            for (int i = 0; i < user_count; i++) {
                if (strcmp(active_users[i].username, receiver) == 0) {
                    receiver_socket = active_users[i].socket_fd;
                    break;
                }
            }
            pthread_mutex_unlock(&user_lock);

            // IF RECEIVER IS ONLINE
            if (receiver_socket != -1) {
                char deliver_msg[1200];
                snprintf(deliver_msg, sizeof(deliver_msg), "Message from %s: %s\n", sender, message);

                send(receiver_socket, deliver_msg, strlen(deliver_msg), 0);

                send(client_socket, "Message delivered.\n", 20, 0);
            } 
            else {
                // Receiver offline → (optional) store for later
                send(client_socket, "User offline. Message not delivered.\n", 38, 0);
            }

        }
        else if (strncmp(buffer, "getmessages", 11) == 0) {
            // TODO Varnika
        }
        else if (strncmp(buffer, "deletemessages", 14) == 0) {
            // TODO Varnika
        }
        else if (strncmp(buffer, "getuserlist", 11) == 0) {
            // TODO Navreet
        }
        else if (strncmp(buffer, "exit", 4) == 0) {
            // TODO Flora: graceful client shutdown
            break;
        }
    }


    // USER CLEANUP AND DISCONNECT (Added Code)
int user_index = -1;  // Search for the user to be removed
pthread_mutex_lock(&user_lock); // Acquire lock for shared resource modification

for (int i = 0; i < user_count; i++) {   // Iterate through active users
    if (active_users[i].socket_fd == client_socket) {   // Match found
        user_index = i;  // Store index for removal
        break;   // Exit loop once user is found
    }
}

if (user_index != -1) {  // Found user, perform cleanup
    printf("[SERVER] Disconnecting user: %s\n", active_users[user_index].username);  // Log disconnection

    // Shift all subsequent elements in the array left by one position
    for (int i = user_index; i < user_count - 1; i++) {   // Shift users to fill the gap
        active_users[i] = active_users[i + 1];   // Overwrite current index with the next user
    }
    user_count--;   // Decrement the user count
}

pthread_mutex_unlock(&user_lock); // Release lock after modification
//END USER CLEANUP (Added Code) 

    close(client_socket);
    return NULL;
}
