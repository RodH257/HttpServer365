#ifndef CONNECTION_QUEUE_H
#define CONNECTION_QUEUE_H
#define QUEUE_SIZE 100 //up to 100 connections can be queued in backlog

#include <pthread.h>
#include <semaphore.h>


/*
* Data structure represents a connection queue
*/
typedef struct connection_queue
{
    //queue of connection FD's
    int connection_queue[QUEUE_SIZE];
    int first, last;

    //synchronization variables
    pthread_mutex_t queue_mutex;
    sem_t slots_free;
    sem_t slots_used;

} connection_queue_t;


void setup_queue(connection_queue_t *queue);
void add_connection(connection_queue_t *queue, int connection);
int get_next_connection(connection_queue_t *queue);


#endif
