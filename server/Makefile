CC = gcc
CFLAGS = -pthread

all: server

server: server.c client_handler.c database.c utils.c message_commands.c
	$(CC) $(CFLAGS) server.c client_handler.c database.c utils.c message_commands.c -o server

clean:
	rm -f server
