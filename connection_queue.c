#include "connection_queue.h"
#include <stdio.h>
#include <stdlib.h>

void setup_queue(connection_queue_t *queue)
{
    //initialize locks
    pthread_mutex_init(&queue->queue_mutex, NULL);
    pthread_mutex_init(&queue->running_check_lock, NULL);
    sem_init(&queue->slots_free, 0, QUEUE_SIZE);
    sem_init(&queue->slots_used, 0, 0);

    queue->first = 0;
    queue->last = QUEUE_SIZE-1;
}

void add_connection(connection_queue_t *queue, int connection)
{

    //wait until there is a free slot
    sem_wait(&queue->slots_free);

    //Acquire lock to protect queue
    pthread_mutex_lock(&queue->queue_mutex);

    //update last
    queue->last = ++queue->last % QUEUE_SIZE;

    //add to queue
    queue->connection_queue[queue->last] = connection;

    printf("Queued connection %d,  next free slot at %d \n", connection, queue->last);

    //signal there's been one added
    sem_post(&queue->slots_used);

    //Release queue lock
    pthread_mutex_unlock(&queue->queue_mutex);
}

int get_next_connection(connection_queue_t *queue)
{
    int cur_value;
    sem_getvalue(&queue->slots_used, &cur_value);

    //block until there's a connection there
    sem_wait(&queue->slots_used);

    //handle the connection
    pthread_mutex_lock(&queue->queue_mutex);

    //get next connection and update queue
    int next_connection = queue->first;
    queue->first = ++queue->first % QUEUE_SIZE;

    sem_post(&queue->slots_free);
    pthread_mutex_unlock(&queue->queue_mutex);

    return next_connection;
}


