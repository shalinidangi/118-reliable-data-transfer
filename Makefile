CC=gcc
all: server client
server: server.c
	$(CC) server.c -O0 -ggdb -o server
client: client.c
	$(CC) client.c -O0 -ggdb -o client
clean:
	rm server client received.data