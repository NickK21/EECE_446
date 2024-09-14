#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT "80"

int lookup_and_connect(const char* host, const char* service);

int main(int argc, char* argv[])
{	
	int sum = 0;
    
	printf("sum: %d\n", sum);
	for (int i = 1; i < argc; i++)
	{
		sum = sum + atoi(argv[i]);
	}
	printf("sum: %d\n", sum);	
	
	char* buf = malloc(31);
    int sockfd;
	
	sockfd = lookup_and_connect("www.ecst.csuchico.edu", SERVER_PORT);
	
	if (sockfd < 0)
	{
		perror("Error connecting to the server\n");
		exit(1);
	}

	char request[] = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";
	int bytes_sent = send(sockfd, request, strlen(request), 0);
	
	if (bytes_sent < 0)
	{
		perror("Error writing to socket\n");
		close(sockfd);
		exit(1);
	}

	long bytes = 0;
	int h1_tags = 0;
	char* h1_start = buf;

	while (bytes_sent != 0)
	{
		//make sure recv can handle multiple receives
		// n = recv(s, buf, sizeof(buf) - 1, 0);
		int bytes_recv = recv(s, buf, 30, 0);
		if (bytes_recv <= 0)
		{
			break;
		}
		buf[bytes_recv] = '\0';
		
		while ((h1_start = strstr(h1_start, "<h1>")) != NULL)
		{
			h1_tags++;
			//Probably need to change, make sure stays within bounds of buf
			h1_start += 4;
		}

		bytes += n;
	}


	printf("Number of <h1> tags: %d\nNumber of bytes: %ld\n", h1_tags, bytes);
	
	if (n < 0)
	{
		perror("Receive Failed");
	}

	free(buf);

	return 0;
}
int lookup_and_connect(const char* host, const char* service) 
{
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = 0;
	hints.ai_protocol = 0;

	if ((s = getaddrinfo(host, service, &hints, &result)) != 0)
    {
		fprintf(stderr, "stream-talk-client: getaddrinfo: %s\n", gai_strerror(s));
		return -1;
	}

	/* Iterate through the address list and try to connect */
	for (rp = result; rp != NULL; rp = rp->ai_next) 
	{
		if ((s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) == -1) 
		{
			continue;
		}

		if (connect(s, rp->ai_addr, rp->ai_addrlen) != -1) 
		{
			break;
		}
		close(s);
	}
    
	if (rp == NULL) 
	{
		perror("stream-talk-client: connect");
        freeaddrinfo(result);
		return -1;
	}
    freeaddrinfo(result);

	return sockfd;
}

