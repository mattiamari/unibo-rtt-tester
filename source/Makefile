.PHONY: all clean

CC = gcc
CFLAGS = -Wall -Werror -Wpedantic -std=c99 -ggdb

all: client server

client: client.h client.c utils.o protocol.o
	$(CC) $(CFLAGS) -o $@ client.c utils.o protocol.o

server: server.h server.c utils.o protocol.o
	$(CC) $(CFLAGS) -o $@ server.c utils.o protocol.o

utils.o: utils.h utils.c
	$(CC) $(CFLAGS) -c utils.c

protocol.o: protocol.h protocol.c
	$(CC) $(CFLAGS) -c protocol.c

clean:
	rm -rf *.o client server