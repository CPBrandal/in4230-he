#include <sys/ioctl.h>   // For ioctl and SIOCGIFHWADDR
#include <net/if.h>      // For ifreq and IFNAMSIZ
#include <unistd.h>      // For close() function
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/if_packet.h>    /* AF_PACKET */
#include <net/ethernet.h>       /* ETH_P_ALL */
#include <arpa/inet.h>          /* htons */
#include <ifaddrs.h>            /* getifaddrs */
#include "mip_arp.h"
#include "pdu.h"
#include "ping.h"
#include "raw_socket.h"
#include "utils.h"

int create_raw_socket(void)
{
    int sd;
    short unsigned int protocol = ETH_P_MIP; /*Ether protocol for sending to mip daemons*/

    /* Set up a raw AF_PACKET socket without ethertype filtering */
    sd = socket(AF_PACKET, SOCK_RAW, htons(protocol));
    if (sd == -1) 
    {
        printf("UNIX RAW PROBLEMS 3");
        perror("socket");
        exit(EXIT_FAILURE);
    }

    return sd;
}


void handle_received_pdu(int raw_socket, struct interface_info *if_list, uint8_t my_mip_address, int unix_socket) 
{
  struct pdu *received_pdu = (struct pdu *)malloc(sizeof(struct pdu)); /*Allocate pdu structure to hold the data*/
    /*Prepare structures for receiving data*/
    struct msghdr *msg;
    struct iovec msgvec[1];
    uint8_t buffer[BUFFER_SIZE];

    struct sockaddr_ll src_addr;
    
    /*The buffer we want to receive to*/
    msgvec[0].iov_base = buffer;
    msgvec[0].iov_len = sizeof(buffer);
    
    msg = (struct msghdr *)calloc(1,sizeof(struct msghdr));
    /*Set up the msghdr structure*/
   
    msg->msg_name = &src_addr;
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iov = msgvec;
    msg->msg_iovlen = 1;

     /*Receive data from the raw socket */
    ssize_t recv_len = recvmsg(raw_socket, msg, 0);
    free(msg);
    if (recv_len == -1) 
    {
        perror("recvmsg");
	destroy_pdu(received_pdu);
        return;
    }

    /*Deserialize the received data into a PDU structure*/
    size_t pdu_size = mip_deserialize_pdu(received_pdu, buffer);
    if(pdu_size < 0) 
    {
        printf("Error deserializing the pdu\n");
        destroy_pdu(received_pdu);
        return;
    }

    if(debug_mode)
    {
        print_pdu_content(received_pdu);
    }

    if (received_pdu->mip_header->sdu_type == MIP_ARP) /*Handle an arp message*/
    {
        /*Cast the pdu to a mip_arp_message struct*/
        struct mip_arp_message *arp_msg = (struct mip_arp_message *)received_pdu->sdu;

        if (arp_msg->type == MIP_ARP_REQUEST) /*Hanlde request*/
        {
            printf("Received MIP-ARP request for MIP address %u\n", arp_msg->address);
            if(my_mip_address == arp_msg->address) /*We only send a response if the message was ment for us*/
            {
                send_arp_response(raw_socket, &src_addr, my_mip_address, received_pdu->mip_header->src_addr, if_list, received_pdu->ether_header->src_addr); /*Includes add to cache*/
            }
        } else if (arp_msg->type == MIP_ARP_RESPONSE) /*Handle response*/
        {
            /*When we receive a response, we know that we have found the target mip address, therfore we can send the ping message*/
            printf("Received MIP-ARP response for MIP address %u\n", arp_msg->address);
            /*We add the details to our cache*/
            add_to_arp_cache(received_pdu->mip_header->src_addr, /*Mip address*/
                            received_pdu->ether_header->src_addr, /*The src-mac address of the message is our dest-mac for the mip*/
                            received_pdu->ether_header->dst_addr); /*The dest-mac address of the message is out src-mac for the mip*/
            /*Allocate pdu for sening and preparing buffer*/
            struct pdu *send_pdu = alloc_pdu();

            uint8_t ping_buffer[sizeof(struct ping_message)];
            size_t ping_buffer_len = sizeof(ping_buffer);
            if(!serialize_ping_message(stored_ping, ping_buffer, &ping_buffer_len)) /* Serialize the stored ping message*/
            {
                printf("Failed to serialize ping");
		destroy_pdu(send_pdu);
                return;
            }
            /*Fill the pdu which we are about to send*/
            fill_pdu(
                send_pdu, 
                received_pdu->ether_header->dst_addr, 
                received_pdu->ether_header->src_addr, 
                my_mip_address, 
                received_pdu->mip_header->src_addr, 
                PING, 
                ping_buffer,
                ping_buffer_len);

            send_pdu_to_raw_socket(raw_socket, send_pdu, if_list); /*Send PDU to correct mip address*/
            
            destroy_pdu(send_pdu);
        }
    } else if (received_pdu->mip_header->sdu_type == PING) 
    {
        if(received_pdu->mip_header->dest_addr == my_mip_address) /*Check if message was for our mip address*/
        {
            /*Because we only send pings to direct neighbours*/
            struct ping_message *ping = (struct ping_message*)received_pdu->sdu;
            print_ping_message(ping);
            send_ping_message_unix(unix_socket, received_pdu->mip_header->src_addr, ping->msg);
            
        } else 
        {
            uint8_t* mac_ad = lookup_mac_dest(received_pdu->mip_header->dest_addr);
            if(mac_ad == NULL)
            {
                /*Set up for future program where we send messages via other mip daemons, call send_arp_request()*/
            } else 
            {
                /*Set up for future program where we send messages via other mip daemons, modify pdu call send_pdu_to_raw_socket()*/
            }
        }
    }
    /*Free any dynamically allocated memory for the received pdu*/
    destroy_pdu(received_pdu);
}

