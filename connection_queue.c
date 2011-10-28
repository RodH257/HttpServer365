#include "connection_queue.h"
#include <stdlib.h>


/*
* Initializes the connection_queue data structure with size
* set in QUEUE_SIZE. Sets up semaphores and mutexes
* PRE: queue has memory allocated
* POST: connection queue will be fully initialized and ready to use
* PARAMS: queue - the connection_queue to initialize
*/
void setup_queue(connection_queue_t *queue)
{
    //initialize locks
    pthread_mutex_init(&queue->queue_mutex, NULL);
    sem_init(&queue->slots_free, 0, QUEUE_SIZE);
    sem_init(&queue->slots_used, 0, 0);

    queue->first = 0;
    queue->last = QUEUE_SIZE-1;
}


/*
* Adds a connection to the queue
* PRE: queue is initialized and connection is active
* POST: connection is added to the next free slot in the queue and queue markers updated
* PARAMS: the queue to add too, connection to add
*/
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

    //signal there's been one added
    sem_post(&queue->slots_used);

    //Release queue lock
    pthread_mutex_unlock(&queue->queue_mutex);
}


/*
* Gets the next connection to process. Blocks until there is one.
* PRE: queue is initialized
* POST: next connection from queue will be returned and queue markers updated
* PARAMS: queue to operate on
*/
int get_next_connection(connection_queue_t *queue)
{
    int cur_value;
    sem_getvalue(&queue->slots_used, &cur_value);

    //block until there's a connection there
    sem_wait(&queue->slots_used);

    //handle the connection
    pthread_mutex_lock(&queue->queue_mutex);

    //get next connection and update queue
    int next_connection = queue->connection_queue[queue->first];
    queue->first = ++queue->first % QUEUE_SIZE;

    sem_post(&queue->slots_free);
    pthread_mutex_unlock(&queue->queue_mutex);

    return next_connection;
}


