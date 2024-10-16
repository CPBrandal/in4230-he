#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include "ping.h"  // Include the ping header for the ping_message structure
#include "utils.h"


#define PING_PREFIX "PING:"
#define PONG_PREFIX "PONG:"


int main(int argc, char *argv[]) 
{
    if (argc != 2) /*Check that we got correct amount of arguments*/
    {
        print_help("ping_server [-h] <socket_lower>");
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "-h") == 0) /*Check if user specified help*/
    {
        print_help("ping_server [-h] <socket_lower>");
        exit(EXIT_SUCCESS);
    }

    const char *socket_path = argv[1];  /*Path to lower socket*/
    struct sockaddr_un addr;

    /*Prepare unix socket*/
    int server_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);  
    if (server_socket == -1) 
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }


    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    /*Connect to the socket created by mipd.c*/
    int rc = connect(server_socket, (struct sockaddr *)&addr, sizeof(addr));
    if (rc == -1) 
    {
        perror("connect");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Ping server connected to the MIP daemon at %s\n", socket_path);

    /*Define buffer, response- and reply message*/
    char buffer[BUFFER_SIZE];
    struct ping_message received_ping;
    struct ping_message pong_response;
    
    while (1) 
    {
        /*Make sure buffer is clean*/
        memset(buffer, 0, BUFFER_SIZE);

        /*Receive data from the mip daemon over unix socket*/
        int bytes_received = recv(server_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) /*In case of success*/
        {
            printf("Received ping message from MIP daemon\n");
            /*DEBUG*/
            printf("Number of bytes received from mip daemon: %d\n", bytes_received);
            if (deserialize_ping_message(&received_ping, (uint8_t*)buffer, bytes_received)) /*Deserialize message*/
            {
                printf("Received ping from MIP address %u: %s\n", received_ping.mip_address, received_ping.msg);

                /*Create pong response*/
                pong_response.mip_address = received_ping.mip_address;
                snprintf(pong_response.msg, sizeof(pong_response.msg), "PONG:%s", received_ping.msg + strlen(PING_PREFIX));

                size_t buffer_len = sizeof(buffer);
                serialize_ping_message(&pong_response, (uint8_t*)buffer, &buffer_len);

                /*Send the serialized pong_message back to mipd*/
                int bytes_sent = send(server_socket, buffer, buffer_len, 0);
                if (bytes_sent == -1) 
                {
                    perror("send");
                } else 
                {
                    printf("Pong sent with message: %s\n", pong_response.msg);
                    /*DEBUG*/
                    printf("Number of bytes sent from server: %d\n", bytes_sent);
                }
            } else /*Error in deserialize*/
            {
                printf("Failed to deserialize ping message\n");
            }
        } else if (bytes_received == -1) /*Error in recv*/
        {
            perror("recv");
            break;
        }
    }

    /*Close socket*/
    close(server_socket);
    printf("Server terminated gracefully");
    return 0;
}
