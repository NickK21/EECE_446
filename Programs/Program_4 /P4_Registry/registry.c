/* EECE-446-FA-2024 | Nick Kaplan | Halin Gailey */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdint.h>
#include <stdbool.h>

// Maximum number of pending connections
#define MAX_PENDING 5            
// Maximum number of peers allowed
#define MAX_PEERS 5              
// Maximum number of files a peer can publish
#define MAX_FILES 10             
// Maximum length of a filename
#define MAX_FILENAME_LEN 128     
// Buffer size for receiving data
#define BUFFER_SIZE 1024         


// Enumeration representing the states of a peer
enum client_state
{
    // Peer has not joined yet
    CLIENT_UNKNOWN,      
    // Peer has joined but not published files
    CLIENT_JOINED,       
    // Peer has published files and is fully registered
    CLIENT_REGISTERED    
};

// Struct to represent peer information
struct PeerData
{
    // Unique ID for the peer
    uint32_t peer_id;                
    // Socket descriptor for the peer
    int peer_socket;                 
    // Peer network address
    struct sockaddr_in peer_addr;    
    // Array of filenames published by the peer
    char** files;                    
    // Number of files published
    int file_count;                  
    // Current state of the peer
    enum client_state state;         
};

// Struct to manage the registry server's state
struct RegistryContext
{
    // Socket descriptor for the registry
    int registry_socket;              
    // Array of peer information
    struct PeerData peers[MAX_PEERS]; 
    // Set of active sockets for select()
    fd_set active_sockets;            
    // Maximum socket descriptor value
    int max_socket;                   
};

// Function prototypes
int initialize_registry_socket(int port);
void monitor_connections(struct RegistryContext* reg_context);
void accept_new_peer(struct RegistryContext* reg_context);
void process_peer_message(struct RegistryContext* reg_context, int peer_socket);
void handle_join(struct RegistryContext* reg_context, int peer_socket, uint32_t peer_id);
void handle_publish(struct RegistryContext* reg_context, int peer_socket, char files[][MAX_FILENAME_LEN], int file_count);
void handle_search(struct RegistryContext* reg_context, int peer_socket, char* search_file);
void send_search(int peer_socket, uint32_t peer_id, struct sockaddr_in* addr);
void cleanup_peer(struct PeerData* peer);

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);

    struct RegistryContext reg_context;
    memset(&reg_context, 0, sizeof(reg_context));
    
    // Initialize the registry socket and prepare to listen
    reg_context.registry_socket = initialize_registry_socket(port);
    FD_SET(reg_context.registry_socket, &reg_context.active_sockets);
    reg_context.max_socket = reg_context.registry_socket;

    printf("Registry server is listening on port %d...\n", port);

    // Monitor and process incoming connections and messages
    monitor_connections(&reg_context);

    close(reg_context.registry_socket);
    return 0;
}

// Initialize a socket for the registry server and start listening for connections
int initialize_registry_socket(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in registry_addr;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Error creating socket");
        exit(1);
    }

    memset(&registry_addr, 0, sizeof(registry_addr));
    registry_addr.sin_family = AF_INET;
    // Bind to all network interfaces
    registry_addr.sin_addr.s_addr = INADDR_ANY;
    // Set the port number
    registry_addr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*)&registry_addr, sizeof(registry_addr)) < 0)
    {
        perror("Error binding socket");
        close(sock);
        exit(1);
    }

    if (listen(sock, MAX_PENDING) < 0)
    {
        perror("Error listening on socket");
        close(sock);
        exit(1);
    }

    return sock;
}
// Monitor sockets for activity and process incoming connections or messages
void monitor_connections(struct RegistryContext* reg_context)
{
    while (1)
    {
        fd_set read_fds = reg_context->active_sockets;
        int ready_sockets = select(reg_context->max_socket + 1, &read_fds, NULL, NULL, NULL);

        if (ready_sockets < 0)
        {
            perror("Select error");
            break;
        }
        // Check each socket for activity
        for (int i = 0; i <= reg_context->max_socket; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == reg_context->registry_socket)
                {
                    // New incoming connection
                    accept_new_peer(reg_context);
                }
                else
                {
                    // Incoming message from an existing peer
                    process_peer_message(reg_context, i);
                }
            }
        }
    }
}

// Accept a new connection and add it to the registry context
void accept_new_peer(struct RegistryContext* reg_context)
{
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    int peer_socket = accept(reg_context->registry_socket, (struct sockaddr*)&peer_addr, &addr_len);

    if (peer_socket < 0)
    {
        perror("Failed to accept connection");
        return;
    }

    // Find an empty slot for the new peer
    for (int i = 0; i < MAX_PEERS; i++)
    {
        // Check if the current slot is available (unused)
        if (reg_context->peers[i].peer_socket == 0)
        {
            // Assign the new peer's socket descriptor
            reg_context->peers[i].peer_socket = peer_socket;
            // Store the new peer's network address
            reg_context->peers[i].peer_addr = peer_addr;
            // Initialize the peer's file list and attributes
             // No files published yet
            reg_context->peers[i].files = NULL;
            // No files have been published
            reg_context->peers[i].file_count = 0;
            // Peer has not joined yet
            reg_context->peers[i].state = CLIENT_UNKNOWN;
            
            // Add the new socket to the set of active sockets for monitoring
            FD_SET(peer_socket, &reg_context->active_sockets);

            // Update the maximum socket descriptor if the new socket is higher
            if (peer_socket > reg_context->max_socket)
            {
                reg_context->max_socket = peer_socket;
            }
            // Log that a new peer connection has been accepted
            printf("Accepted new peer connection\n");
            return;
        }
    }

    // Reject connection if the max number of peers is reached
    printf("Reached max peer limit\n");
    close(peer_socket);
}

void cleanup_peer(struct PeerData* peer)
{
    // Check if the peer has allocated memory for files
    if (peer->files != NULL)
    {
        // Free each filename in the files array
        for (int i = 0; i < peer->file_count; i++)
        {
            free(peer->files[i]);
        }
        // Free the array holding the file pointers
        free(peer->files);
    }
    // Set the files pointer to NULL to avoid dangling references
    peer->files = NULL;
    // Reset the file count to 0 since the files have been cleared
    peer->file_count = 0;
    // Reset the peer's state to CLIENT_UNKNOWN, marking it as unregistered
    peer->state = CLIENT_UNKNOWN;
    // Reset the peer's unique ID
    peer->peer_id = 0;
    // Close the peer's socket (if open) and reset the socket descriptor to 0
    peer->peer_socket = 0;
    // Clear the peer's network address structure
    memset(&peer->peer_addr, 0, sizeof(peer->peer_addr));
}

// Process a message from a peer and handle its command
void process_peer_message(struct RegistryContext* reg_context, int peer_socket)
{
    uint8_t command;
    // Read the command byte from the peer's socket
    ssize_t bytes_received = recv(peer_socket, &command, sizeof(command), 0);

    // Handle socket closure or errors during receive
    if (bytes_received <= 0)
    {
        if (bytes_received < 0)
        {
            perror("Error receiving data");
        }
        else
        {
            printf("Peer disconnnected\n");
        }
        close(peer_socket);
        FD_CLR(peer_socket, &reg_context->active_sockets);
        return;
    }

    // Locate the peer and process the command
    for (int i = 0; i < MAX_PEERS; i++) 
    {
        if (reg_context->peers[i].peer_socket == peer_socket) 
        {
            // Ensure the peer has joined before processing non-JOIN commands
            if (reg_context->peers[i].state == CLIENT_UNKNOWN && command != 0) 
            {
                printf("Error: Peer must JOIN before other actions\n");
                return;
            }

            // Handle commands from the peer
            switch (command)
            {
                // JOIN
                case 0:
                {
                    uint32_t peer_id;
                    // Read the peer ID from the socket
                    if (recv(peer_socket, &peer_id, sizeof(peer_id), 0) <= 0) 
                    {
                        perror("Error receiving JOIN data");
                        close(peer_socket);
                        FD_CLR(peer_socket, &reg_context->active_sockets);
                        return;
                    }
                    handle_join(reg_context, peer_socket, ntohl(peer_id));
                    break;
                }
                // PUBLISH
                case 1:
                {
                    if (reg_context->peers[i].state != CLIENT_JOINED) 
                    {
                        printf("Error: Peer must JOIN before publishing\n");
                        return;
                    }                 
                    
                    int file_count;
                    // Read the number of files to be published
                    if (recv(peer_socket, &file_count, sizeof(file_count), 0) <= 0) 
                    {
                        perror("Error receiving PUBLISH file count");
                        close(peer_socket);
                        FD_CLR(peer_socket, &reg_context->active_sockets);
                        return;
                    }
                    file_count = ntohl(file_count);
                    // Buffer to hold filenames
                    char files[MAX_FILES][MAX_FILENAME_LEN];
                    // Temporary buffer for received data
                    char temp_buffer[BUFFER_SIZE];
                    int total_received = 0;
                    int filenames_parsed = 0;

                    // Continue receiving and parsing filenames until all are received
                    while (filenames_parsed < file_count)
                    {
                        int bytes_received = recv(peer_socket, temp_buffer + total_received, BUFFER_SIZE - total_received, 0);

                        if (bytes_received <= 0)
                        {
                            perror("Error reveiving PUBLISH file names");
                            close(peer_socket);
                            FD_CLR(peer_socket, &reg_context->active_sockets);
                            return;
                        }

                        total_received += bytes_received;
                        // Tracks the current position in the buffer
                        int offset = 0;
                        while (offset < total_received && filenames_parsed < file_count)
                        {
                            // Look for a null terminator to mark the end of a filename
                            char* null_pos = memchr(temp_buffer + offset, '\0', total_received - offset);
                            if (null_pos != NULL)
                            {
                                // Calculate the length of the filename
                                int filename_len = null_pos - (temp_buffer + offset);
                                if (filename_len >= MAX_FILENAME_LEN)
                                {
                                    fprintf(stderr, "Filename exceeds maximum allowed length\n");
                                    close(peer_socket);
                                    FD_CLR(peer_socket, &reg_context->active_sockets);
                                    return;
                                }
                                // Copy the filename into the files array
                                strncpy(files[filenames_parsed], temp_buffer + offset, filename_len);
                                files[filenames_parsed][filename_len] = '\0';
                                filenames_parsed++;

                                // Move the offset past the null terminator
                                offset += filename_len + 1;
                            }
                            else
                            {
                                // Shift unprocessed data to the start of the buffer
                                memmove(temp_buffer, temp_buffer + offset, total_received - offset);
                                // Adjust total_received to reflect remaining data
                                total_received -= offset;
                                break;
                            }
                        }
                    }
                    printf("Finished collecting peer files\n");
                    handle_publish(reg_context, peer_socket, files, file_count);
                    break;
                }
                //SEARCH
                case 2:
                {
                    if (reg_context->peers[i].state != CLIENT_REGISTERED) 
                    {
                        printf("Error: Peer must publish files before searching\n");
                        return;
                    }

                    char search_file[MAX_FILENAME_LEN];
                    // Read the filename to search for
                    if (recv(peer_socket, search_file, MAX_FILENAME_LEN, 0) <= 0) 
                    {
                        perror("Error receiving SEARCH file name");
                        close(peer_socket);
                        FD_CLR(peer_socket, &reg_context->active_sockets);
                        return;
                    }
                    handle_search(reg_context, peer_socket, search_file);
                    break;
                }
                default:
                    printf("Unknown command received\n");
                    break;
            }
            break;
        }
    }
}

// Handle the JOIN command from a peer
void handle_join(struct RegistryContext* reg_context, int peer_socket, uint32_t peer_id)
{
    // Iterate through the peers array to locate the requesting peer
    for (int i = 0; i < MAX_PEERS; i++)
    {
        // Check if this peer's socket matches the requesting peer's socket
       if (reg_context->peers[i].peer_socket == peer_socket)
       {
            // Assign the provided peer ID to the peer
            reg_context->peers[i].peer_id = peer_id;
            // Update the peer's state to CLIENT_JOINED
            reg_context->peers[i].state = CLIENT_JOINED;

            printf("TEST] JOIN %u\n", peer_id);

            printf("Peer %d joined with ID %u\n", peer_socket, peer_id);
            return;
       } 
    }
}

// Handle the PUBLISH command from a peer
void handle_publish(struct RegistryContext* reg_context, int peer_socket, char files[][MAX_FILENAME_LEN], int file_count)
{
    
    printf("Handling Publish\n");

    for (int i = 0; i < MAX_PEERS; i++) 
    {
        if (reg_context->peers[i].peer_socket == peer_socket)
        {
            if (reg_context->peers[i].state != CLIENT_JOINED) 
            {
                fprintf(stderr, "Error: Peer must JOIN before publishing files\n");
                return;
            }

            if (file_count > MAX_FILES)
            {
                fprintf(stderr, "Error: Too many files, max allowed is %d\n", MAX_FILES);
                return;
            }
            // Free previously published files (if any)
            if (reg_context->peers[i].files != NULL)
            {
                for (int j = 0; j < reg_context->peers[i].file_count; j++)
                {
                    free(reg_context->peers[i].files[j]);
                }
                free(reg_context->peers[i].files);
            }
            // Allocate memory for new files
            reg_context->peers[i].files = calloc(file_count, sizeof(char*));
            if (reg_context->peers[i].files == NULL)
            {
                perror("Failed to allocate memory for files");
                return;
            }

            reg_context->peers[i].file_count = file_count;

            // Copy file names to the peer's file list
            for (int j = 0; j < file_count; j++)
            {
                reg_context->peers[i].files[j] = strdup(files[j]);
                if (reg_context->peers[i].files[j] == NULL)
                {
                    perror("Failed to allocate memory for file name");
                    for (int k = 0; k < j; k++)
                    {
                        free(reg_context->peers[i].files[k]);
                    }
                    free(reg_context->peers[i].files);
                    reg_context->peers[i].files = NULL;
                    return;
                }
            }

            reg_context->peers[i].state = CLIENT_REGISTERED;
            
            // Generate the exact output expected by the test script
            printf("TEST] PUBLISH %d", file_count);
            for (int j = 0; j < file_count; j++) 
            {
                printf(" %s", reg_context->peers[i].files[j]);
            }
            // Ensure only one newline at the end of the output
            printf("\n");  

            return;
        }
    }
    fprintf(stderr, "handle_publish: peer not found\n");
}

// Handle the SEARCH command from a peer
void handle_search(struct RegistryContext* reg_context, int peer_socket, char* search_file)
{
    // Iterate through the peers array to locate the requesting peer
    for (int i = 0; i < MAX_PEERS; i++)
    {
       // Check if this peer's socket matches the requesting peer's socket
       if (reg_context->peers[i].peer_socket == peer_socket) 
       {
            // Ensure the requesting peer has published files (registered)
            if (reg_context->peers[i].state != CLIENT_REGISTERED) 
            {
                fprintf(stderr, "Error: Peer must publish files before searching\n");
                return;
            }
            // Search through all registered peers for the requested file
            for (int j = 0; j < MAX_PEERS; j++) 
            {
                // Only consider peers that have registered files
                if (reg_context->peers[j].state == CLIENT_REGISTERED) 
                {
                    // Check each file published by the current peer
                    for (int k = 0; k < reg_context->peers[j].file_count; k++) 
                    {
                        // If the file matches the search query, send the result
                        if (strcmp(reg_context->peers[j].files[k], search_file) == 0) 
                        {
                            // Send search result with matching peer's ID and address
                            send_search(peer_socket, reg_context->peers[j].peer_id, &reg_context->peers[j].peer_addr);
                            printf("TEST] SEARCH %s %u %s:%d\n", search_file, reg_context->peers[j].peer_id,
                                   inet_ntoa(reg_context->peers[j].peer_addr.sin_addr),
                                   ntohs(reg_context->peers[j].peer_addr.sin_port));
                            return;
                        }
                    }
                }
            }

            // If no match is found, send a "not found" response
            send_search(peer_socket, 0, NULL);
            printf("TEST] SEARCH %s 0 0.0.0.0:0\n", search_file);
            return;
       }
    }
    fprintf(stderr, "handle_search: peer not found\n");
}

void send_search(int peer_socket, uint32_t peer_id, struct sockaddr_in* addr)
{
    // Buffer to hold the response message (10 bytes)
    uint8_t response[10] = {0};

    // If a valid peer ID and address are provided, populate the response
    if (peer_id != 0 && addr != NULL)
    {
        // Convert peer ID to network byte order
        uint32_t net_id = htonl(peer_id);
        // Get the peer's IP address
        uint32_t net_addr = addr->sin_addr.s_addr;
        // Get the peer's port number
        uint16_t net_port = addr->sin_port;

        // Copy the data into the response buffer
        // First 4 bytes: Peer ID
        memcpy(response, &net_id, sizeof(net_id));
        // Next 4 bytes: Peer IP address
        memcpy(response + 4, &net_addr, sizeof(net_addr));
        // Last 2 bytes: Peer port number
        memcpy(response + 8, &net_port, sizeof(net_port)); 
    }

    // Send the response back to the peer
    if (send(peer_socket, response, sizeof(response), 0) < 0)
    {
        perror("Error sending search response");
    }
}
