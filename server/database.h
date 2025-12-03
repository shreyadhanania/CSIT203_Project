#ifndef DATABASE_H
#define DATABASE_H

// Store message in DB (timestamped). msg may contain spaces.
void db_store_message(const char *sender, const char *receiver, const char *msg);

// Retrieve all messages between two users and write results to client_fd.
// Each message will be printed with its stored timestamp.
void db_get_messages(const char *user1, const char *user2, int client_fd);

// Delete all messages between two users
void db_delete_messages(const char *user1, const char *user2);

// Check if any message history exists between two users
int db_user_exists_in_history(const char *user1, const char *user2);

#endif
