#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

#define BACKLOG 10
#define RCVBUFSIZE 1024
#define SENDBUFSIZE 1024
#define MAXPATHSIZE 1024
#define DOCS_PATH "htdocs/"
#define QUEUE_SIZE 30

size_t readline(int socket, char* buf, size_t size);
void write_line(int socket, char* str);
void sigint_handler(int sig);


void connection_handler (int connection_fd);
void *connection_consumer (void *param);

int http_socket_fd; //the file descriptor for main socket
int server_port; //port that it is listening on

//Thread pool variables
int connection_queue[QUEUE_SIZE];
int keep_running;
pthread_mutex_t queue_mutex;
pthread_mutex_t running_check_lock;
sem_t slots_free;
sem_t slots_used;
int insert_pointer = 0, remove_pointer = 0;


void *connection_consumer (void *param)
{
    while (true)
    {
        int curValue;
        sem_getvalue(&slots_used, &curValue);

        printf("Waiting for connection \n");
        //block until there's a connection there
        sem_wait(&slots_used);
printf("Connection there \n");
        //check if we are to exit
        //get the running_check_lock
        pthread_mutex_lock(&running_check_lock);
        if (!keep_running)
        {
            //we're exiting.
            pthread_mutex_unlock(&running_check_lock);
            //post to slots_free so that producer thread isnt blocked
            sem_post(&slots_free);
            return 0;
        }

        //still running, give back the lock and carry on
        pthread_mutex_unlock(&running_check_lock);

        //handle the connection
        pthread_mutex_lock(&queue_mutex);

        int connection_fd  = connection_queue[remove_pointer++];
        remove_pointer = remove_pointer % QUEUE_SIZE;
        connection_queue[remove_pointer] = -1;

        sem_post(&slots_free);
        pthread_mutex_unlock(&queue_mutex);

        printf("Handling connection %d \n", connection_fd);
        //handle connection
        connection_handler(connection_fd);


        //now sleep for 0.5 seconds
        //  struct timespec sleepTime;
        //  struct timespec returnTime;
        //  sleepTime.tv_sec = 0;
        //  sleepTime.tv_nsec = 500000000;
        //   nanosleep(&sleepTime, &returnTime);
    }
    return 0;
}

//Handles certain connection
void connection_handler(int connection_fd)
{
    char buffer[RCVBUFSIZE];
    printf("receiving \n");
    if (readline(connection_fd, buffer, RCVBUFSIZE) > 0)
    {
        printf("Received %s", buffer);
    }

    int numchars = 1;
    //buffer to be discarded
    char discardBuffer[RCVBUFSIZE] = "Buffer";

    //get rid of headers
    while ((numchars > 0) && strcmp("\n", discardBuffer))  /* read & discard headers */
        numchars = readline(connection_fd, discardBuffer, RCVBUFSIZE);


    char response200[] = "HTTP/1.0 200 OK\r\n";
    char response404[] = "HTTP/1.0 404 NOT FOUND\r\n";
    char headers[] = "Server: HttpServer365\r\n"
                     "Content-Type: text/html\r\n"
                     "\r\n";

    //check its a GET request
    if (strstr(buffer, "GET ") == buffer)
    {
        //its a GET request
        printf("GET request \n");

        //all files under htdocs, so append this to path
        char filePath[MAXPATHSIZE] = DOCS_PATH;

        int path_index = 4;
        while (!isspace((int)buffer[path_index]) && path_index < (MAXPATHSIZE - 7))
        {
            filePath[(path_index+2)] = buffer[path_index];
            path_index++;
        }

        //asked for default path, so set it to be index.html
        if (strcmp(filePath, DOCS_PATH) == 0)
        {
            strcat(filePath, "index.html");
        }
        printf("Reading file %s \n", filePath);


        //open file for reading
        FILE *fileData = fopen(filePath, "r");
        if (fileData == NULL)
        {
            //not found, 404
            printf("File not found\n");
            write_line(connection_fd, response404);
            write_line(connection_fd, headers);
            write_line(connection_fd, "<html><body>404 Not Found</body></html>\r\n\r\n");
        }
        else
        {
            char fileBuffer[RCVBUFSIZE];
            printf("Found file \n");
            //send headers
            write_line(connection_fd, response200);
            write_line(connection_fd, headers);
            //read it to the send buffer
            while (!feof(fileData))
            {
                fgets(fileBuffer, sizeof(fileBuffer), fileData);
                write_line(connection_fd, fileBuffer);
            }

            write_line(connection_fd, "\r\n\r\n");
        }


        printf("Done \n");
        close(connection_fd);
    }

}


int main(int argc, char *argv[])
{
    //check that port was supplied
    if (argc <2)
    {
        printf("You must supply a port number for web server. Perhaps 80? \n");
        exit(0);
    }

    server_port = atoi(argv[1]);

    //create sigint handler
    signal(SIGINT, sigint_handler);


    //initialize locks
    pthread_mutex_init(&queue_mutex, NULL);
    pthread_mutex_init(&running_check_lock, NULL);
    sem_init(&slots_free, 0, QUEUE_SIZE);
    sem_init(&slots_used, 0, 0);

    //create consumer thread
    pthread_t consumerThread;
    keep_running = true;

    if (pthread_create(&consumerThread, NULL, connection_consumer, NULL))
    {
        //there was an error creating the consumer
        printf("Error creating consumer thread");
        abort();
    }




    //setup socket
    int connection_fd;
    struct sockaddr_in their_addr; //client addres
    struct sockaddr_in my_addr;  //server address
    socklen_t sin_size;

    //create a socket
    if ((http_socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket Creation Failed");
        exit(1);
    }

    int reuse_socket = true;
    if (setsockopt(http_socket_fd,SOL_SOCKET,SO_REUSEADDR,&reuse_socket,sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }


    my_addr.sin_family = AF_INET;         //via IP
    my_addr.sin_port = htons(server_port);  //HTTP port
    my_addr.sin_addr.s_addr = INADDR_ANY; //listen on any connection

    //bind the socket
    if (bind(http_socket_fd, (struct sockaddr *)&my_addr,
             sizeof(struct sockaddr))	== -1)
    {
        perror("Unable to Bind Socket");
        exit(1);
    }


    //start listening on the socket
    if (listen(http_socket_fd, BACKLOG) == -1)
    {
        perror("Unable to Listen on Socket");
        exit(1);
    }

    printf("HTTP Server now listening....\n");



    //accept connections
    while(1)
    {
        //make sure there is room on the queue
        int curValue;
        sem_getvalue(&slots_free, &curValue);

        sin_size = sizeof(struct sockaddr_in);

        if (curValue <= 0)
            printf("Connection queue is full.\n");

        //wait until there is a free berth
        sem_wait(&slots_free);

        //accept new connection
        if ((connection_fd = accept(http_socket_fd, (struct sockaddr *)&their_addr,
                                    &sin_size)) == -1)
        {
            perror("Unable to accept connection");
            continue;
        }

        //connection accepted
        printf("server: got connection from %s\n",
               inet_ntoa(their_addr.sin_addr));

        //add the connection to the queue for threads to process
        //Acquire lock to protect queue
        pthread_mutex_lock(&queue_mutex);

        //choose smallest queue position to insert into
        connection_queue[insert_pointer++] = connection_fd;
        insert_pointer = insert_pointer % QUEUE_SIZE;

        printf("Queued connection %d,  next free slot at %d \n", connection_fd, insert_pointer);

        //signal there's been one added
        sem_post(&slots_used);

        //Release queue lock
        pthread_mutex_unlock(&queue_mutex);

    }
    close(connection_fd);

    if (pthread_join(consumerThread, NULL))
    {
        printf("error joining consumer");
        abort();
    }

    //clean up any child processes
    while(waitpid(-1,NULL,WNOHANG) > 0);
}




//reads a line from the socket
//line can end in \r, \r\n or \n and it will stop there.
//if line is bigger than the buffer, it will end there as well.
size_t readline(int socket, char* buffer, size_t size)
{
    int i = 0;
    char c = '\0';
    int recv_size;

    while((i < size-1) && (c != '\n'))
    {
        //receive one character at a time from the socket
        recv_size = recv(socket, &c, 1, 0);

        if (recv_size > 0)
        {
            //receive successfull
            //check for carriage return
            if (c == '\r')
            {
                //got start of carriage return, check for newline
                //MSG_PEEK lets us read the next data without affecting the position
                recv_size = recv(socket, &c, 1, MSG_PEEK);
                if ((recv_size >0) && (c == '\n'))
                {
                    //we've got a crlf, lets actually receive that last \n
                    recv(socket, &c, 1, 0);
                }
                else
                {
                    //convert the carriage return start into a \n so we will exit the loop
                    c = '\n';
                }
            }
            buffer[i] = c;
            i++;
        }
        else
        {
            c = '\n';
        }
    }

    //terminate it with null
    buffer[i] = '\0';
    return i;
}


void write_line(int socket, char* str)
{
    // char* newString = strcat(str, "\r\n");
    printf("Sending response %s", str );
    if (send(socket, str,(int)strlen(str), 0) == -1)
        perror("Unable to send line");
}

void sigint_handler(int sig)
{
    printf("\nClosing Socket....\n");

    //clean up any child processes
    while(waitpid(-1,NULL,WNOHANG) > 0);
    close(http_socket_fd);

    exit(0);
}
