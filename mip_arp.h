#ifndef MIP_ARP_H
#define MIP_ARP_H

#include <stdint.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include "local_interfaces.h"
#include "pdu.h"

/*There are 255 mip addresses, but 255 is reserved, and we use one space leaving 253 addresses left*/
#define MAX_ARP_CACHE_SIZE 253 

#define MIP_ARP_REQUEST 0
#define MIP_ARP_RESPONSE 1

#define ETH_BROADCAST_ADDR {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}

/*Struct for an arp entry in our arp_list. Contains mip, dest mac address and source mac address.*/
struct arp_entry {
    uint8_t mip_address;   /*Destination MIP address*/
    uint8_t mac_address[6]; /*Destination mac address*/
    uint8_t src_mac_address[6]; /*Source mac address, the one we use to send*/ 
} __attribute__((packed));

/*Struct for a MIP-ARP message. Contains type (0 or 1), address (MIP) and reserved (padding)*/
struct mip_arp_message {
    uint8_t type;          /* 1 bit for Type (0 for MIP_ARP_REQUEST, 1 for MIP_ARP_RESPONSE)*/
    uint8_t address;       /*8 bits for MIP address*/
    uint32_t reserved;     /*Padding/Reserved (set to 0)*/
} __attribute__((packed));

/*Global variables for the list of arp_entries and the count of the list*/
extern struct arp_entry arp_list[MAX_ARP_CACHE_SIZE];
extern int arp_cache_count;

/*ARP message functions:*/

/*Function to send an arp response over raw socket. It allocates and fills a PDU after finding which interface the previous message was received on. Lastly it deallocate the pdu by calling destroy().
Function takes the raw socked fd, a pointer to an interface, source and destination mip address, a pointer to interface_info and the destination mac address as parameters.*/
void send_arp_response(int raw_socket, struct sockaddr_ll *so_name, uint8_t mip_address, uint8_t target_mip_address, struct interface_info *if_list, uint8_t dest_addr[6]);


/*Function to send an arp request over raw socket. The function sets dest address to broadcast address, and for each local interfaces it sends an arp request message, finally 
it deallocates the PDU by calling destroy().
The function takes a raw socket fd, source and destination mip address and a pointer to interface_info as parameters.*/
void send_arp_request(int raw_socket, struct interface_info *if_list, uint8_t mip_address, uint8_t src_mip_address);


/*Arp cache management:*/

/*Function to initialize the arp_cache*/
void initialize_arp_cache(); 


/*Function to find the destination mac address based on the corresponding mip address in the arp list, 
it matches the mip address given with the ones stored in arp_list and returns the destination mac address.
Function takes a mip address as a parameter.
Returns either correct mac address or null.*/
uint8_t *lookup_mac_dest(uint8_t mip_address);


/*Function to find the source mac address based on the corresponding mip address in the arp list, 
it matches the mip address given with the ones stored in arp_list and returns the source mac address.
Function takes a mip address as a parameter.
Returns either correct mac address or null.*/
uint8_t *lookup_mac_src(uint8_t mip_address);


/*Function checks if the arp_list has reaced its max number of entries, if not it adds mip address, mac dest address and mac src address.
Funtion takes a mip address, mac dest address and mac source address as parameters.
*/
void add_to_arp_cache(uint8_t mip_address, uint8_t mac_address[6], uint8_t src_mac[6]);

#endif // MIP_ARP_H
