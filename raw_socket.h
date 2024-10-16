#ifndef RAW_SOCKET_H
#define RAW_SOCKET_H

#include <stdint.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include "local_interfaces.h"
#include "pdu.h"
#include "mip_arp.h"

/*Ethertype for mip traffic*/
#define ETH_P_MIP 0x88B5 

/*Struct for the mip header, contains mip-dest, mip-src, time to live, sdu length and sdu type*/
struct mip_header {
    uint8_t dest_addr;
    uint8_t src_addr;
    uint8_t ttl : 4;
    uint16_t sdu_len : 9;
    uint8_t sdu_type : 3;
} __attribute__((packed));

/*Struct for ether header, contains dest-mac, src-mac and ethernet protocol */
struct ether_frame {
	uint8_t dst_addr[6];
	uint8_t src_addr[6];
	uint16_t eth_proto;
} __attribute__((packed));


/*Creates a raw socket which is used for sending data between MIPs. 
Returns the socket descriptor.
Uses ETH_P_MIP which is defined in raw_socket.h.*/
int create_raw_socket(void);


/*Function allocates a PDU and receives data via raw socket from another MIP which it deserializes into the PDU, it differenciates between different type of
SDUs and performs actions accordingly. If the data received is of type MIP-ARP, it checks whether it is a request or a response.
For request it checks if the request was for its MIP-address and if so it calls send_arp_response().
For response, it is implied that the response is an answere to a request we have sent, and also that we only get a response if we sent to correct MIP, 
meaning we can send our PING packet.
Therefore we call add_to_arp_cache(), prepare a PDU for sending, fills it with the stored_ping, call send_pdu_to_raw_socket() and call destroy() for PDU.
For PING message it prepares a ping, and call send_ping_unix_socket().
Function takes the raw_socket, interface list, the mip address of the host's MIP and the unix_socket fd for sending over unix as parameters.
Dependent on the global variable debug_mode.*/
void handle_received_pdu(int raw_socket, struct interface_info *if_list, uint8_t my_mip_address, int unix_socket);

#endif
