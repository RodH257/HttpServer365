CC = c99
CLFAGS = -Wall -g -pthread
LDFLAGS = -lpthread

all: server client

server: server.o connection_queue.o

client: client.o

server.o: server.c

client.o: client.c

connection_queue.o: connection_queue.c

clean:
	rm -f fib *.o

.PHONY: clean
