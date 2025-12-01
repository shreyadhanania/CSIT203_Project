CC = gcc
CFLAGS = -pthread

all: server

server: server.c client_handler.c database.c utils.c
	$(CC) $(CFLAGS) server.c client_handler.c database.c utils.c -o server

clean:
	rm -f server
