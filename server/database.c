#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // write()
#include <pthread.h>
#include <time.h>

#include "database.h"
#include "utils.h"

// Storage file
const char *DB_FILE = "messages.db";
static pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

// Row format:
// TIMESTAMP|SENDER|RECEIVER|MESSAGE\n
// example: 2025-02-04 19:48:23|alice|bob|Hello Bob!

void db_store_message(const char *sender, const char *receiver, const char *msg) {
    pthread_mutex_lock(&db_lock);

    FILE *fp = fopen(DB_FILE, "a");
    if (!fp) {
        pthread_mutex_unlock(&db_lock);
        return;
    }

    char timestamp[64];
    current_timestamp(timestamp, sizeof(timestamp));

    // Ensure newline in msg is removed
    char clean_msg[1024];
    strncpy(clean_msg, msg, sizeof(clean_msg)-1);
    clean_msg[sizeof(clean_msg)-1] = '\0';
    trim_newline(clean_msg);

    fprintf(fp, "%s|%s|%s|%s\n", timestamp, sender, receiver, clean_msg);
    fclose(fp);

    pthread_mutex_unlock(&db_lock);
}

void db_get_messages(const char *user1, const char *user2, int client_fd) {
    pthread_mutex_lock(&db_lock);

    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) {
        write(client_fd, "No messages.\n", 13);
        pthread_mutex_unlock(&db_lock);
        return;
    }

    char line[2048];
    int found_any = 0;
    while (fgets(line, sizeof(line), fp)) {
        // parse: timestamp|sender|receiver|message
        char timestamp[64], s[50], r[50], msg[1200];
        // initialize to empty
        timestamp[0]=s[0]=r[0]=msg[0]=0;

        // Use sscanf with scansets to handle | separators
        sscanf(line, "%63[^|]|%49[^|]|%49[^|]|%1199[^\n]", timestamp, s, r, msg);

        if ((strcmp(s, user1)==0 && strcmp(r,user2)==0) ||
            (strcmp(s, user2)==0 && strcmp(r,user1)==0)) {

            found_any = 1;

            char output[1600];
            // Show direction based on sender
            if (strcmp(s, user1) == 0) {
                // user1 sent to user2
                snprintf(output, sizeof(output), "[%s] %s -> %s: %s\n", timestamp, s, r, msg);
            } else {
                // user1 received from user2 (s == user2)
                snprintf(output, sizeof(output), "[%s] %s -> %s: %s\n", timestamp, s, r, msg);
            }
            write(client_fd, output, strlen(output));
        }
    }
    fclose(fp);

    if (!found_any) {
        write(client_fd, "No message history found.\n", 26);
    }

    pthread_mutex_unlock(&db_lock);
}

void db_delete_messages(const char *user1, const char *user2) {
    pthread_mutex_lock(&db_lock);

    FILE *fp = fopen(DB_FILE, "r");
    FILE *temp = fopen("temp.db", "w");

    if (!fp || !temp) {
        if (fp) fclose(fp);
        if (temp) fclose(temp);
        pthread_mutex_unlock(&db_lock);
        return;
    }

    char line[2048];
    while (fgets(line, sizeof(line), fp)) {
        char timestamp[64], s[50], r[50], msg[1200];
        timestamp[0]=s[0]=r[0]=msg[0]=0;

        sscanf(line, "%63[^|]|%49[^|]|%49[^|]|%1199[^\n]", timestamp, s, r, msg);

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

    pthread_mutex_unlock(&db_lock);
}

int db_user_exists_in_history(const char *user1, const char *user2) {
    pthread_mutex_lock(&db_lock);

    FILE *fp = fopen(DB_FILE, "r");
    if (!fp) {
        pthread_mutex_unlock(&db_lock);
        return 0;
    }

    char line[2048];
    int found = 0;
    while (fgets(line, sizeof(line), fp)) {
        char timestamp[64], s[50], r[50], msg[1200];
        timestamp[0]=s[0]=r[0]=msg[0]=0;

        sscanf(line, "%63[^|]|%49[^|]|%49[^|]|%1199[^\n]", timestamp, s, r, msg);

        if ((strcmp(s, user1)==0 && strcmp(r,user2)==0) ||
            (strcmp(s, user2)==0 && strcmp(r,user1)==0)) {
            found = 1;
            break;
        }
    }

    fclose(fp);
    pthread_mutex_unlock(&db_lock);
    return found;
}
