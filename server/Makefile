CC = gcc
CFLAGS = -pthread

all: server

server: server.c client_handler.c database.c utils.c message_commands.c user_list.c
	$(CC) $(CFLAGS) server.c client_handler.c database.c utils.c message_commands.c user_list.c -o server

clean:
	rm -f server
