#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024


int main(int argc, char *argv[])
{
    int sockfd, numbytes, server_port;
    char buffer[BUFFER_SIZE];
    struct hostent *he;
    struct sockaddr_in their_addr;

    if (argc != 3)
    {
        fprintf(stderr,"Please supply hostname and port\n");
        exit(1);
    }

    //get the host by name
    if ((he=gethostbyname(argv[1])) == NULL)
    {
        perror("gethostbyname");
        exit(1);
    }

    //read the port to connect on
    server_port = atoi(argv[2]);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Failed to initialize socket");
        exit(1);
    }

    their_addr.sin_family = AF_INET;
    their_addr.sin_port = htons(server_port);
    their_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);
    //fill with nulls
    memset(their_addr.sin_zero, '\0', sizeof their_addr.sin_zero);


    if (connect(sockfd, (struct sockaddr *)&their_addr, \
                sizeof(struct sockaddr)) == -1)
    {
        perror("Unable to connect socket");
        exit(1);
    }

    //send request for /
    char* send_message = "GET / HTTP/1.0 \n\r\n";

    if (send(sockfd, send_message,(int)strlen(send_message), 0) == -1)
        perror("Unable to send GET request");

    printf("Sent %s \n", send_message);
    numbytes = 1;
    bool first = true;
    while ((numbytes > 0))
    {
        numbytes = recv(sockfd, buffer, (BUFFER_SIZE-1), 0);
        if (first){
           printf("Received: \n");
        }
        //if we got no response at all, print nothign
        if (first && numbytes == 0)
        {
            printf("Nothing! \n");
        }
        else
        {
            buffer[numbytes] = '\0';
            printf("%s",buffer);
        }
        first = false;
    }

    close(sockfd);
    return 0;
}
