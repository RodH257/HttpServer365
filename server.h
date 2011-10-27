#ifndef SERVER_H
#define SERVER_H
#include <sys/types.h>

size_t readline(int connection, char* buf, size_t size);
void write_line(int connection, char* str);
void sigint_handler(int sig);
void connection_handler (int connection_fd);
void *connection_consumer (void *param);

#endif
