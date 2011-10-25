#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUFFER_SIZE 1024


int main(int argc, char *argv[])
{
	int sockfd, numbytes, server_port;
	char buffer[MAX_DATA_SIZE];
	struct hostent *he;
	struct sockaddr_in their_addr;

	if (argc != 3) {
		fprintf(stderr,"Please supply hostname and port\n");
		exit(1);
	}

  //get the host by name
	if ((he=gethostbyname(argv[1])) == NULL) {
		herror("gethostbyname");
		exit(1);
	}

  //read the port to conncet on
  server_port = atoi(argv[1]);

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Failed to initialize socket");
		exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(server_port);
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8);

	if (connect(sockfd, (struct sockaddr *)&their_addr, \
	sizeof(struct sockaddr)) == -1) {
		perror("Unable to connect");
		exit(1);
	}

	if ((numbytes=recv(sockfd, buffer, MAXDATASIZE, 0)) == -1) {
		perror("Unable to receive data");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("Response: %s",buffer);
	close(sockfd);
	return 0;
}
