#include <stdio.h>      // For printf and perror
#include <stdlib.h>     // For exit
#include <string.h>     // For memset and strncpy
#include <unistd.h>     // For close and unlink
#include <sys/socket.h> // For socket, bind, listen, and AF_UNIX
#include <sys/un.h>     // For struct sockaddr_un (Unix domain sockets)
#include "utils.h"

int debug_mode = 0;


int create_unix_socket(const char *path) 
{
    int unix_socket;
    struct sockaddr_un addr;

    /*We use SOCK_SEQPACKET for reliablity*/
    unix_socket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (unix_socket == -1) 
    {
        printf("UNIX SOCKET PROBLEMS 1");
        perror("socket");
        exit(EXIT_FAILURE);
    }
    /*We make sure the addr is clean*/
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX; /*We bind the socket to the socket name*/
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1); /*- 1 to exclude the nullbyte*/

    unlink(path);  /*We ensure theres is no old socket file*/

    if (bind(unix_socket, (struct sockaddr *)&addr, sizeof(addr)) == -1) 
    {
        printf("UNIX SOCKET PROBLEMS 2");
        perror("bind");
        close(unix_socket);
        exit(EXIT_FAILURE);
    }

    /*Prepare for incoming connections*/
    if (listen(unix_socket, MAX_BACKLOG) == -1) 
    {
        perror("listen");
        close(unix_socket);
        exit(EXIT_FAILURE);
    }

    return unix_socket;
}


void print_help(const char *message)
{   
    printf("%s\n",message);
}