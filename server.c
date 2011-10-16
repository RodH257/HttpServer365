#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h>


#define HTTP_PORT 80
#define BACKLOG 10

int main(int argc, char *argv[])
{
	int sockfd, new_fd;  /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr;  //server address
	struct sockaddr_in their_addr; //client address
	socklen_t sin_size;

	//create a socket
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket Creation Failed");
		exit(1);
	}

	
	my_addr.sin_family = AF_INET;         //via IP
	my_addr.sin_port = htons(HTTP_PORT);  //HTTP port
	my_addr.sin_addr.s_addr = INADDR_ANY; //listen on any connection

	//bind the socket
	if (bind(sockfd, (struct sockaddr *)&my_addr, 
	sizeof(struct sockaddr))	== -1) {
		perror("Unable to Bind Socket");
		exit(1);
	}
	
	//start listening on the socket
	if (listen(sockfd, BACKLOG) == -1) {
		perror("Unable to Listen on Socket");
		exit(1);
	}

	printf("HTTP Server now listening....\n");

	//accept connections
	while(1) { 
		sin_size = sizeof(struct sockaddr_in);
		
		//accept new connection
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
		&sin_size)) == -1) {
			perror("Unable to accept connection");
			continue;
		}
		
		//connection accepted 
		printf("server: got connection from %s\n", 
			inet_ntoa(their_addr.sin_addr));
		
		if (!fork()) { 
			//child process, serve result
			if (send(new_fd, "Hello, world!\n", 14, 0) == -1)
				perror("send");
			close(new_fd);
			exit(0);
		}
		close(new_fd);  
		
		//clean up any child processes
		while(waitpid(-1,NULL,WNOHANG) > 0); 
	}


}
