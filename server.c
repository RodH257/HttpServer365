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
#include "connection_queue.h"

#define BACKLOG 10
#define RCVBUFSIZE 1024
#define SENDBUFSIZE 1024
#define MAXPATHSIZE 1024
#define DOCS_PATH "htdocs/"
#define THREAD_POOL_SIZE 30

size_t readline(int socket, char* buf, size_t size);
void write_line(int socket, char* str);
void sigint_handler(int sig);


void connection_handler (int connection_fd);
void *connection_consumer (void *param);

int http_socket_fd; //the file descriptor for main socket
int server_port; //port that it is listening on

pthread_t thread_pool[THREAD_POOL_SIZE];
connection_queue_t *connection_queue;


void *connection_consumer (void *param)
{
    while (true)
    {
        int connection_fd = get_next_connection(connection_queue);

        printf("Handling connection %d \n", connection_fd);
        //handle connection
        connection_handler(connection_fd);
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
    while ((numchars > 0) && strcmp("\n", discardBuffer) && strcmp("\n", buffer))
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

    //create connection queue
    connection_queue = (connection_queue_t*)malloc(sizeof(connection_queue_t));
    setup_queue(connection_queue);
    //create consumer threads
    int thread_index = 0;
    while (thread_index < THREAD_POOL_SIZE)
    {
        pthread_t consumer_thread;
        if (pthread_create(&consumer_thread, NULL, connection_consumer, NULL))
        {
            //there was an error creating the consumer
            perror("Error creating consumer thread");
            exit(1);
        }
        thread_pool[thread_index] = consumer_thread;
        thread_index++;
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
        //todo: test to see if perhaps we should wait here for new slot before accepting connection

        sin_size = sizeof(struct sockaddr_in);
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

        //add it to the queue
        add_connection(connection_queue,  connection_fd);

    }
    close(connection_fd);
    thread_index = 0;
    while (thread_index < THREAD_POOL_SIZE)
    {
        if (pthread_join(thread_pool[thread_index], NULL))
        {
            printf("error joining consumer");
            abort();
        }
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
