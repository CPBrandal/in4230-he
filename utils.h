#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>

/*/*Since the SDU length is 9 bits it can hold 511 * 4 = 2044 bytes, 
however I have defined the ping message buffer to hold 256 characters due to the nature of the task (PING/PONG messages). 
It is not defined how much data our program have to be able to send/receive.*/
#define BUFFER_SIZE 1024

/*Value to determine how many connections the unix socket will queue.
Set to 24 for now, which i believe will suffice.*/
#define MAX_BACKLOG 24


/*Global variable to indicate whether program is in debug mode or not*/
extern int debug_mode;


/*Function to create a unix socket for sending data between application and mip daemon (mipd) uses sock_seq*/
int create_unix_socket(const char *path);


/*Helper function to print help message for running executable programs (mipd.c, ping_client.c and ping_server.c)
Takes a poiner to a const char as parameter.*/
void print_help(const char *message);

#endif
