/* EECE-446-FA-2024 | Nick Kaplan | Halin Gailey */ 

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>

#define SERVER_PORT "80"

// Function prototypes
int lookup_and_connect(const char* host, const char* service);
int sendall(int s, char* buf, int* len);
int recvall(int s, char* buf, int* len);


int main(int argc, char* argv[])
{	
	// Buffer for receiving data
	char buf[1024];
	// Socket file descriptor
	int sockfd;
	// Chunk size for receiving data
	int count = 0;

	// Validate input arguments
    if (argc == 2)
    {
		// Convert command-line argument to integer (chunk size)
    	count = atoi(argv[1]);
    }
	else
    {
    	perror("Invalid Chunk Size, Try Again");
		// Exit if no valid argument is provided
    	exit(1);
  	}

	sockfd = lookup_and_connect("www.ecst.csuchico.edu", SERVER_PORT);
	
	if (sockfd < 0)
	{
		perror("Error connecting to the server\n");
		exit(1);
	}
	 // HTTP GET request to fetch specific file from the server
	char request[] = "GET /~kkredo/file.html HTTP/1.0\r\n\r\n";


  	int send_len = strlen(request);
	int bytes_sent = sendall(sockfd, request, &send_len);
	
	if (bytes_sent < 0)
	{
		perror("Error writing to socket\n");
		close(sockfd);
		exit(1);
	}

	// Variables to track received bytes and count <h1> tags in the response
	int bytes = 0;
	int h1_tags = 0;
	int bytes_recv = 0;
	// Pointer to chunk size
  	int* len = &count;

	// Receive data from the server in chunks and count <h1> tags
	while ((bytes_recv = recvall(sockfd, buf, len)) > 0)
	{
		// Null-terminate the received data for string operations
		buf[bytes_recv] = '\0';
		 // Pointer to traverse through the buffer
	    char* h1_start = buf;
		
		while ((h1_start = strstr(h1_start, "<h1>")) != NULL)
		{
			h1_tags++;
			// Move pointer past the current <h1> tag
      		h1_start += strlen("<h1>");
		}

		bytes += bytes_recv;

		printf("Number of <h1> tags: %d\n", h1_tags);
	
		printf("Number of bytes: %d\n", bytes);
	
		if (sockfd < 0)
		{
			perror("Receive Failed");
		}

		return 0;
	}
}

int sendall(int s, char* buf, int* len)
{
	int total = 0;
	int bytes_left = *len;
	int n;

	while (total < *len)
	{
		n = send(s, buf + total, bytes_left, 0);
		if (n == -1)
		{
			break;
		}
		// Update total bytes sent
		total += n;
		// Update remaining bytes
		bytes_left -= n;
	}
	// Update len with total bytes sent
	*len = total;

	// Return -1 if sending failed, otherwise return 0
	return n == -1 ? -1 : 0;
}

int recvall(int s, char* buf, int* len)
{
	int total = 0;
	int bytes_left = *len;
	int n;

	while (total < *len)
	{
		n = recv(s, buf + total, bytes_left, 0);
		if (n == -1 || n == -0)
		{
			break;
		}
		total += n;
		bytes_left -= n;
	}
	*len = total;

	return n == -1 ? -1 : total;	
}
int lookup_and_connect(const char* host, const char* service) 
{
	struct addrinfo hints;
	struct addrinfo *rp, *result;
	int s;

	/* Translate host name into peer's IP address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
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
		return 