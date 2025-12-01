#ifndef DATABASE_H
#define DATABASE_H

// Store message in DB
void db_store_message(const char *sender, const char *receiver, const char *msg);

// Retrieve all messages between two users
void db_get_messages(const char *user1, const char *user2, int client_fd);

// Delete all messages between two users
void db_delete_messages(const char *user1, const char *user2);

#endif
