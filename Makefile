CC = c99
CLFAGS = -Wall -g -pthread
LDFLAGS = -lpthread

all: server

server: server.o connection_queue.o

server.o: server.c

connection_queue.o: connection_queue.c

clean:
	rm -f fib *.o

.PHONY: clean
