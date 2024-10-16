#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <errno.h>
#include "ping.h"  // Include the ping header for the ping_message structure
#include "utils.h"

#define PING_PREFIX "PING:"

int main(int argc, char *argv[]) 
{
    if (argc != 4) /*Check that we got correct amount of arguments*/
    {
        print_help("Usage: ping_client [-h] <socket_lower> <destination_host> <message>");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "-h") == 0) /*Check if user specified help*/
    {
        print_help("Usage: ping_client [-h] <socket_lower> <destination_host> <message>");
        exit(EXIT_SUCCESS);
    }

    const char *socket_path = argv[1]; /*Path to lower socket*/
    uint8_t destination_host = (uint8_t)atoi(argv[2]); /*Mip destination, converted*/
    const char *user_message = argv[3]; /*Message to be sent*/

    /*Add PING to the message*/
    char message_with_prefix[BUFFER_SIZE];
    snprintf(message_with_prefix, sizeof(message_with_prefix), "%s%s", PING_PREFIX, user_message);

    int client_socket;
    struct sockaddr_un server_addr;
    int rc = 0;

    /*Prepare unix socket*/
    client_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (client_socket == -1) 
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, socket_path, sizeof(server_addr.sun_path) - 1);

    /*Connect to the socket created by mipd.c*/
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) 
    {
        perror("connect");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Ping client connected to the MIP daemon at %s\n", socket_path);

    /*Initializeing ping message to be sent*/
    struct ping_message ping;
    memset(&ping, 0, sizeof(ping));
    init_ping_message(&ping, destination_host, message_with_prefix);

    /*Serialize ping into buffer*/
    uint8_t buffer[BUFFER_SIZE];
    size_t buffer_len = sizeof(buffer);

    if (!serialize_ping_message(&ping, buffer, &buffer_len)) 
    {
        printf("Failed to serialize the ping message.\n");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    /*Structure for measuring time*/
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    /*Send the serialized message over unix socket*/
    rc = send(client_socket, buffer, buffer_len, 0);
    if (rc == -1) 
    {  // Use send instead of sendto
        perror("send");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    printf("Ping sent to MIP address %u with message: %s\n", destination_host, user_message);
    /*DEBUG*/
    printf("Number of bytes sent from client: %d\n", rc);
    struct timeval tv;
    tv.tv_sec = 1;  /*Timeout is 1 sec*/
    tv.tv_usec = 0;

    if (setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == -1) /*Set timout option to socket*/
    {
        perror("setsockopt");
        close(client_socket);
        exit(EXIT_FAILURE);
    }

    /*Clean buffer and receive message*/
    memset(buffer, 0, BUFFER_SIZE);
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);

    /*Measure end time*/
    gettimeofday(&end_time, NULL);

    if (bytes_received > 0) /*In case of success*/
    {
        printf("Received ping message from MIP daemon\n");
        /*DEBUG*/
        printf("Number of bytes received from mip daemon: %d\n", bytes_received);
        struct ping_message response;
        if (deserialize_ping_message(&response, buffer, bytes_received)) /*Deserialize*/
        {
            /*We check whether the respons is a PONG message*/
            if (strncmp(response.msg, "PONG:", 5) == 0) 
            {
                /*Response without prefix*/
                const char *response_content = response.msg + 5;
                /*Compare the response with the message we sent without prefixes*/
                if (strcmp(response_content, user_message) == 0)
                {
                    /*Calculate the time it took*/
                    long seconds = end_time.tv_sec - start_time.tv_sec;
                    long microseconds = end_time.tv_usec - start_time.tv_usec;
                    double elapsed = seconds + microseconds * 1e-6;

		            print_ping_message(&response);
                    printf("Round-trip time: %.6f seconds\n", elapsed);
                } else /*Response is not equal to the one we sent*/
                {
                    printf("Received PONG response, but the message does not match the original.\n");
		            print_ping_message(&response);
                }
            } else /*Received a non-pong message*/
            {
                printf("Received invalid response (non-pong): %s\n", response.msg);
            }
        } else /*Failed to deserialize the response*/
        {
            printf("Failed to deserialize the response.\n");
        }
    } else if (errno == EAGAIN || errno == EWOULDBLOCK) /*Timeout*/
    {
        printf("Timeout waiting for PONG response\n");
        close(client_socket);
        return 0;
    } else /*Error receiving packet*/
    {   
        printf("Client could not receive message.\n");
        perror("recv");
    }

    /*Close socket*/
    close(client_socket);
    return 0;
}
