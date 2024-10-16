#ifndef PING_H
#define PING_H

#include <stdint.h>
#include "pdu.h"

/*Struct for a ping message, includes the mip address and message*/
struct ping_message 
{
    uint8_t mip_address;  /*MIP address of the sender/receiver*/
    char msg[256];        /*Message content (e.g., "PING:<message>")*/
};

/*Global variable for containing the PING message received from client/server*/
extern struct ping_message *stored_ping;

/*Function to initialize a ping message. Function fills the appropriate values with values provided by caller.
Takes a pointer to a struct ping_message, a mip address and a const char *message as parameters.
Returns nothing. */
void init_ping_message(struct ping_message *ping, uint8_t mip_address, const char *message);


/*Function to serialize a struct ping_message into a byte stream. Function changes the buffer_len to the actual size of 
the struct.
Takes a pointer to struct ping_message, a buffer and a pointer to a size_t length as parameters.
Returns 1 on success and 0 on failure.*/
int serialize_ping_message(const struct ping_message *ping, uint8_t *buffer, size_t *buffer_len);


/*Function to deserialize a byte stream into a struct ping_message.
Takes a pointer to struct ping_message, a buffer and a size_t length as parameters.
Returns 1 on success and 0 on failure.*/
int deserialize_ping_message(struct ping_message *ping, uint8_t *buffer, size_t buffer_len);


/*Function to send a ping over the unix socket.
Takes unix socket fd, mip address and message as parameters.*/
void send_ping_message_unix(int unix_socket, uint8_t mip_address, const char *message);


/*Function to print the content of a struct ping_message. Prints the mip address and the message (sdu).
Takes a pointer to ping_message struct as parameter.*/
void print_ping_message(struct ping_message *ping);


/*Function to store a given struct ping_message into the global variable stored_ping.
It frees the previously stored ping_message and allocates memory for the new ping_message and stores it in stored_message.
Takes a pointer to a struct ping_message as parameter.
Dependent on global variable stored_ping and debug_mode.
Returns nothing but prints appropriate error messages.*/
void store_ping_message(struct ping_message *msg);

#endif // PING_H
