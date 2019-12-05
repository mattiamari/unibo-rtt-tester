.PHONY: all clean

CC = gcc
CFLAGS = -Werror -Wall -Wpedantic -Wextra -std=c99

all: CFLAGS += -O3
all: client server

debug: CFLAGS += -ggdb
debug: client server

client: client.c utils.o protocol.o
	$(CC) $(CFLAGS) -o $@ client.c utils.o protocol.o

server: server.c utils.o protocol.o
	$(CC) $(CFLAGS) -o $@ server.c utils.o protocol.o

utils.o: utils.h utils.c
	$(CC) $(CFLAGS) -c utils.c

protocol.o: protocol.h protocol.c
	$(CC) $(CFLAGS) -c protocol.c

clean:
	rm -rf *.o client server