.PHONY: all clean make_build_dir

CC = gcc
CFLAGS = -Wall -Werror -Wpedantic -std=c99 -ggdb

BUILD_DIR = ./bin


all: client server

client: client.h client.c
	$(CC) $(CFLAGS) -o $@ client.c

server : server.h server.c
	$(CC) $(CFLAGS) -o $@ server.c

clean:
	rm -rf *.o client server