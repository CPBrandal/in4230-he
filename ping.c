#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ping.h"
#include "utils.h"

struct ping_message *stored_ping = NULL;

void init_ping_message(struct ping_message *ping, uint8_t mip_address, const char *message) 
{
    /*Set mip address*/
    ping->mip_address = mip_address;

    /*Copy the message into ping->msg*/
    strncpy(ping->msg, message, sizeof(ping->msg) - 1);

    /*Ensure null byte*/
    ping->msg[sizeof(ping->msg) - 1] = '\0';
}


int serialize_ping_message(const struct ping_message *ping, uint8_t *buffer, size_t *buffer_len) 
{
    /*Make sure the buffer has enough space*/
    size_t message_length = strlen(ping->msg);
    if (*buffer_len < 1 + message_length + 1) /*1 byte for mip address + message length + null terminatio (1 byte)*/
    { 
        return 0;
    }

    buffer[0] = ping->mip_address;

    memcpy(buffer + 1, ping->msg, message_length + 1); /*Serialize the message, include null byte*/

    *buffer_len = 1 + message_length + 1;    /*Modify the buffer_len to represent the actual size */

    return 1;
}


int deserialize_ping_message(struct ping_message *ping, uint8_t *buffer, size_t buffer_len) 
{
    if (buffer_len < 1) /*Check if buffer is large enough to hold at least mip address*/
    {
        return 0;
    }

    ping->mip_address = buffer[0]; /*The first byte is the mip address*/

    size_t msg_len = buffer_len - 1;  /*Remaining part is the message*/

    if (msg_len >= sizeof(ping->msg))  /*Make sure the message does not exceed the maximum size for ping->msg*/
    {
        msg_len = sizeof(ping->msg) - 1;  /*Leave room for the null byte terminator*/
    }

    /*Copy the message*/
    memcpy(ping->msg, buffer + 1, msg_len);
    ping->msg[msg_len] = '\0';  /*Ensure null-terminated string*/

    return 1;
}


void send_ping_message_unix(int unix_socket, uint8_t mip_address, const char *message) 
{
    /*Prepare ping*/
    struct ping_message ping;
    memset(&ping, 0, sizeof(ping));

    ping.mip_address = mip_address;
    snprintf(ping.msg, sizeof(ping.msg), "%s", message);

    if (send(unix_socket, &ping, strlen(message) + 3, 0) == -1) /*Send ping message over unix socket*/
    {
        if(debug_mode)
        {
            printf("Error in send_ping_message_unix()");
        }
        perror("send");
    } else 
    {
        printf("Ping message succesfully sent from mip daemon to application.\n");
        printf("Sent ping message: %s from MIP address: %d\n", message, mip_address);
    }
}


void print_ping_message(struct ping_message *ping) 
{
  if(ping == NULL)
  {
    printf("The provided ping_message is null.\n"); /*Prints error msg*/
  } else 
  {
    printf("Ping message: mip address: %u\n", ping->mip_address); /*Print mip address*/
    printf("Message: %s\n", ping->msg); /*Print content (sdu)*/
  }
}


void store_ping_message(struct ping_message *ping) 
{
    if(ping == NULL)
    {
        printf("Error in store_ping_message: the struct provided is null.\n"); /*Prints error msg*/
    } else 
    {
        if (stored_ping != NULL) /*Free stored ping message if it exists*/
        {
            free(stored_ping);
	    stored_ping = NULL;
        }
        /*Allocate memory for the stored_ping*/
        stored_ping = (struct ping_message *)malloc(sizeof(struct ping_message));
        if (stored_ping != NULL) 
        {
            memcpy(stored_ping, ping, sizeof(struct ping_message)); /*Copy new ping_message into global variable stored_ping*/
            if(debug_mode) 
            {
                printf("Storing completed, stored ping_message: MIP Address: %u, Message: %s\n", stored_ping->mip_address, stored_ping->msg);
            }
        } else 
        {
            perror("malloc failed");
        }
    }
}
