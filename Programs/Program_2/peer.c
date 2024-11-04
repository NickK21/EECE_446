#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <dirent.h>
#include <stdint.h>

#define MAX_BUFFER_SIZE 1024
#define SERVER_PORT 5000

int lookup_and_connect(const char* host, const char* service);
void join(uint32_t peerID, int sockfd);
void publish(int sockfd);
void search(int sockfd);
void close_program(int sockfd);
void display_options(uint32_t peerID, int socket);

int main(int argc, char* argv[])
{
    
    char regIP[MAX_BUFFER_SIZE];
    char regPNumber[SERVER_PORT];
    uint32_t pID;
    int sockfd;

    // Validate input arguments
    if (argc == 4)
    {
        // Convert command line argument to integer
        strncpy(regIP, argv[1], MAX_BUFFER_SIZE);
        strncpy(regPNumber, argv[2], MAX_BUFFER_SIZE);
        pID = atoi(argv[3]);
    }
    else
    {
        perror("Error Input");
        exit(1);
    }
    // Attempt to connect to the registry using the provided IP address and port number
    sockfd = lookup_and_connect(regIP, regPNumber);

    if (sockfd < 0)
    {
        perror("Failed To Connect To Registry\n");
        exit(1);
    }

    // Display available options to the user, passing in the peer ID and socket file descriptor
    display_options(pID, sockfd);

    return 0;
}   

void join(u_int32_t peerID, int sockfd)
{
    // Buffer for JOIN request
    unsigned char buf[5];
    // Action code for JOIN is 0
    buf[0] = 0;
    // Write Peer ID to buf    
    uint32_t network_order_peer_id = htonl(peerID);
    memcpy(buf + 1, &network_order_peer_id, sizeof(network_order_peer_id));

    // Send JOIN request
    if (send(sockfd, buf, sizeof(uint32_t) + 1, 0) < 0)
    {
        perror("Send Failed\n");
        return;
    }
    printf(" JOIN Request Sent. Peer ID: %u\n", peerID);
}

// Publishes information about the files located in "SharedFiles" directory to the registry
void publish(int sockfd)
{
    // Holds information sent to the registry
    unsigned char buf[MAX_BUFFER_SIZE];
    // Counts the number of regular files (not directories) found in "SharedFiles" directory
    uint32_t count = 0;
    // Pointer to store the directory stream
    DIR* dirent;
    // Calls opendir to open "SharedFiles" (where files to be published are stored) 
    dirent = opendir("SharedFiles");
    // Pointer to iterate through each file in the directory
    struct dirent *dirpointer;
    // 5 represents current position in buf array where files will be written, first 5 bytes reserved for action code + file count
    size_t iterator = 5;

    if (!dirent)
    {
        perror("Error Opening Directory\n");
        return;
    }
    // Action code
    buf[0] = 1;
    
    // Read files from directory
    while ((dirpointer = readdir(dirent)) != NULL)
    {
        // Checks if the current entry is a regular file, DT_REG represents regular files
        if (dirpointer->d_type == DT_REG)
        {
            // Length of current file's name
            size_t name_len = strlen(dirpointer->d_name);
            if (iterator + name_len + 1 > MAX_BUFFER_SIZE)
            {
                fprintf(stderr, "Buffer Overflow, Too Many Files.\n");
                closedir(dirent);
                return;
            }
            // Ensures enough space in the buffer 
            // Current file’s name is copied into the buffer starting at position iterator 
            // strlen(dirpointer->d_name) + 1 ensures that the null terminator is also copied.
            memcpy(buf + iterator, dirpointer->d_name, strlen(dirpointer->d_name) + 1);
            // Points to the next available position in the buffer
            // Size increment is name_len + 1 to account for the file name and its null terminator.
            iterator += name_len + 1;
            count++;
        }
    }
    
    closedir(dirent);
    // Convert file count into network byte order
    uint32_t network_order_count = htonl(count);
    memcpy(buf + 1, &network_order_count, sizeof(network_order_count));
    if (send(sockfd, buf, iterator, 0) < 0)
    {    
        perror("Error Sending PUBLISH");
        return;
    }
    printf("PUBLISH Request Sent. File Count: %u\n", count);
}

void search(int sockfd)
{
    // Create buffers for:
    // Name of file that user wants to search for
    // Buffer to hold the message to be sent to the server (search command and filename)
    char filename[MAX_BUFFER_SIZE];
    unsigned char buf[MAX_BUFFER_SIZE];
    printf("Enter a file name: ");
    fgets(filename, MAX_BUFFER_SIZE, stdin);
    filename[strcspn(filename, "\n")] = 0;

    // Action code 
    buf[0] = 2;
    // Copies filename buffer into buf
    memcpy(buf + 1, filename, strlen(filename) + 1);

    if (send(sockfd, buf, strlen(filename) + 2, 0) < 0) 
    {
        perror("Error Sending SEARCH");
        return;
    }
    // Buffer to hold server's response
    unsigned char response[10];
    int received = recv(sockfd, response, sizeof(response), 0);
 
    if (received <= 0)
    {
        perror("Error Receiving Response");
        return;
    }
    // First 4 bytes of the response buffer are interpreted as the peer ID of the peer that holds the requested file.
    uint32_t peer_id;
    memcpy(&peer_id, &response[0], sizeof(peer_id));
    peer_id = ntohl(peer_id); 
    
    if (peer_id == 0)
    {
        printf("File Not Indexed By Registry\n");
    }
    else
    {
        // Extract IP address and port number of peer that holds the file
        char ip[INET_ADDRSTRLEN];
        // Stores of IP address of peer
        uint32_t ip_addr;
        // Converts binary IP address into readable string, stores in ip
        memcpy(&ip_addr, &response[4], sizeof(ip_addr));        
        inet_ntop(AF_INET, &ip_addr, ip, INET_ADDRSTRLEN);
        // Stores port number of peer
        uint16_t port;
        memcpy(&port, &response[8], sizeof(port));
        port = ntohs(port);

        // Prints peer ID, IP address, and port number that hold requested file
        printf("File found at\n Peer %u\n", peer_id);
        printf("%s:%u\n", ip, port);
    }
}

void close_program(int sockfd)
{
    close(sockfd);
    printf("Peer Application Exited.\n");
    exit(0);
}

void display_options(uint32_t  peerID, int sockfd)
{
    char command[MAX_BUFFER_SIZE];

    while(1)
    {
        printf("Enter a command: ");

        fgets(command, MAX_BUFFER_SIZE, stdin);
        
        //Remove trailing newline that fgets uses
        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "JOIN") == 0)
        {
            join(peerID, sockfd);
        }
        else if (strcmp(command, "PUBLISH") == 0)
        {
            publish(sockfd);
        }
        else if (strcmp(command, "SEARCH") == 0)
        {
            search(sockfd);
        }
        else if (strcmp(command, "EXIT") == 0)
        {
            close_program(sockfd);
        }
        else
        {
            printf("Invalid Command, Please use JOIN, PUBLISH, SEARCH, or EXIT.\n");
        }
    }

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
        return -1;
    }
    freeaddrinfo(result);

    return s;
}
