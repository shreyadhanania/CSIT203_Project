#ifndef MESSAGE_COMMANDS_H
#define MESSAGE_COMMANDS_H

void handle_getmessages(int client_socket, char *raw_command);
void handle_deletemessages(int client_socket, char *raw_command);

#endif
