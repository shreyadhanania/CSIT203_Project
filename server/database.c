#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // write()
#include <string.h> // strlen()
#include "database.h"


// Simple message struct
typedef struct {
    char sender[50];
    char receiver[50];
    char message[1024];
} Message;

// Storage file
const char *DB_FILE = "messages.db";

void db_store_message(const char *sender, const char *receiver, const char *msg) {
    FILE *fp = fopen(DB_FILE, "a");
    if (!fp) return;

    fprintf(fp, "%s|%s|%s\n", sender, receiver, msg);
    fclose(fp);
}

void db_get_messages(const char *user1, const char *user2, int client_fd) {
    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) {
        write(client_fd, "No messages.\n", 13);
        return;
    }

    char line[1200];
    while (fgets(line, sizeof(line), fp)) {
        char s[50], r[50], msg[1024];
        sscanf(line, "%49[^|]|%49[^|]|%1023[^\n]", s, r, msg);

        if ((strcmp(s, user1)==0 && strcmp(r,user2)==0) ||
            (strcmp(s, user2)==0 && strcmp(r,user1)==0)) {

            char output[1300];
            snprintf(output, sizeof(output), "%s -> %s: %s\n", s, r, msg);
            write(client_fd, output, strlen(output));
        }
    }
    fclose(fp);
}

void db_delete_messages(const char *user1, const char *user2) {
    FILE *fp = fopen(DB_FILE, "r");
    FILE *temp = fopen("temp.db", "w");

    if (!fp || !temp) return;

    char line[1200];
    while (fgets(line, sizeof(line), fp)) {
        char s[50], r[50], msg[1024];
        sscanf(line, "%49[^|]|%49[^|]|%1023[^\n]", s, r, msg);

        // Skip messages matching user1<->user2
        if ((strcmp(s, user1)==0 && strcmp(r, user2)==0) ||
            (strcmp(s, user2)==0 && strcmp(r, user1)==0)) {
            continue;
        }

        fputs(line, temp);
    }

    fclose(fp);
    fclose(temp);

    remove(DB_FILE);
    rename("temp.db", DB_FILE);
}

int db_user_exists_in_history(const char *user1, const char *user2) {
    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) return 0;

    char line[1200];
    while (fgets(line, sizeof(line), fp)) {
        char s[50], r[50], msg[1024];
        sscanf(line, "%49[^|]|%49[^|]|%1023[^\n]", s, r, msg);

        if ((strcmp(s, user1)==0 && strcmp(r,user2)==0) ||
            (strcmp(s, user2)==0 && strcmp(r,user1)==0)) {
            fclose(fp);
            return 1;   // FOUND history
        }
    }

    fclose(fp);
    return 0; // NO history
}
