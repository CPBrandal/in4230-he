#ifndef _PDU_H_
#define _PDU_H_

#include <stdint.h>
#include <stddef.h>

#include "raw_socket.h"

/*Types of messages*/
#define PING 0x02
#define MIP_ARP 0x01

/*Struct for PDU, containing the ether header, mip header and an SDU.*/
struct pdu {
	struct ether_frame *ether_header;
	struct mip_header *mip_header;
	uint8_t *sdu;
} __attribute__((packed));

/*Function to allocate memory for a pdu.
Function takes no parameters, but returns a pointer to the SDU struct.*/
struct pdu *alloc_pdu(void);


/*Function to fill the pdu with details given as parameters.
Function takes a pointer to a struct pdu, a pointer to the source mac address, a pointer to the dest mac address, 
the source mip address, the destination mip address, the type (PING/MIP_ARP), a pointer to the sdu and the pdu size as parameters.
*/
void fill_pdu(struct pdu *pdu,
	      uint8_t *src_mac_addr,
	      uint8_t *dst_mac_addr,
	      uint8_t src_mip_addr,
	      uint8_t dst_mip_addr,
          uint8_t type,
	      uint8_t *sdu, /*Must be 32 bit alligned*/
          uint8_t pdu_size);


/*Function to serialize PDU into a byte stream for sending. 
It fills the buffer and keeps track of the size of the entire pdu.
Takes a pointer to a pdu struct and a pointer to a buffer as parameters.
The function returns the size of the buffer/pdu.*/
size_t mip_serialize_pdu(struct pdu*, uint8_t *buffer);


/*Function to deserialize a byte stream into a struct pdu for receiveing.
It fills the given pdu based on the buffer provided by the caller, and allocates appropriate memory.
Takes a pointer to a pdu struct and a pointer to a buffer as parameters.*/
size_t mip_deserialize_pdu(struct pdu*, uint8_t *buffer);


/*Helper function to print the content of the pdu, 
including the details of the ether header, the mip header and the sdu.
Function takes a pointer to a pdu struct as a parameter.*/
void print_pdu_content(struct pdu*);


/*A funtion to deallocate the memory allocated in relation to the PDU structure. 
Uses free().
Takes a pointer to struct pdu as a parameter.*/
void destroy_pdu(struct pdu*);


/*Helper function to print a mac address.
Takes a pointer to a MAC address and the length which is always 6.
It returns nothing, but prints the MAC address.*/
void print_mac_addr(uint8_t *mac_addr, size_t length);


/*Takes a raw socket fd, a pointer to a pdu struct and a pointer to an interface_info struct as parameters.
Dependent on the global variable for debug_mode.
If debug_mode is activated then it calls print_pdu_content.
*/
void send_pdu_to_raw_socket(int raw_socket, struct pdu *send_pdu, struct interface_info *if_list);

#endif /* _PDU_H_ */