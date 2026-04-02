CC = gcc
CFLAGS = -Wall -Wextra -std=c11
PORT = 4242

all: server player

server: server.c game_ops.c game_ops.h game_entities.h
	$(CC) $(CFLAGS) -DPORT=$(PORT) server.c game_ops.c -o server

player: player.c game_ops.h game_entities.h
	$(CC) $(CFLAGS) -DPORT=$(PORT) player.c -o player

clean:
	rm -f server player

.PHONY: all clean