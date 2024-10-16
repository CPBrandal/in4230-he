#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mip_arp.h"
#include "raw_socket.h"  // For sending MIP packets
#include "pdu.h"
#include "utils.h"


/*Define arp_list and count*/
struct arp_entry arp_list[MAX_ARP_CACHE_SIZE];
int arp_cache_count = 0;

void initialize_arp_cache() 
{
    memset(arp_list, 0, sizeof(arp_list));  /*Clear the ARP cache list*/
    arp_cache_count = 0;                    /*Set cache count to 0*/
    printf("ARP cache initialized.\n");
}


uint8_t *lookup_mac_dest(uint8_t mip_address) /*For larger networks we would need a more efficient search algorithm*/
{
    for (int i = 0; i < arp_cache_count; i++) /*Iterate through arp_list for each entry*/
    {
        if (arp_list[i].mip_address == mip_address) /*Check if mip address is a match*/
        {
            return arp_list[i].mac_address; /*Return correct destination mac address*/
        }
    }
    /*If not found we return NULL*/
    return NULL;
}


uint8_t *lookup_mac_src(uint8_t mip_address) /*For larger networks we would need a more efficient search algorithm*/
{
    for (int i = 0; i < arp_cache_count; i++) /*Iterate through arp_list for each entry*/
    {
        if (arp_list[i].mip_address == mip_address) /*Check if mip address is a match*/
        {
            return arp_list[i].src_mac_address; /*Return correct source mac address*/
        }
    }
    /*If not found we return NULL*/
    return NULL;
}


void add_to_arp_cache(uint8_t mip_address, uint8_t dest_mac[6], uint8_t src_mac_address[6]) 
{
    if (arp_cache_count < MAX_ARP_CACHE_SIZE) /*Check that we have room for more arp_entries*/
    {
        arp_list[arp_cache_count].mip_address = mip_address; /*Set mip address*/
        memcpy(arp_list[arp_cache_count].mac_address, dest_mac, 6); /*Set destination mac address*/
        memcpy(arp_list[arp_cache_count].src_mac_address, src_mac_address, 6); /*Set source mac address*/
        arp_cache_count++;                                                   /*Update count*/
        printf("ARP cache size: %d\n", arp_cache_count);
    } else 
    {
        printf("ARP cache is full, can not add to cache.\n");
    }
}


void send_arp_request(int raw_socket, struct interface_info *if_list, uint8_t mip_address, uint8_t src_mip_address) 
{
    struct mip_arp_message arp_request;
    struct pdu *pdu_request;
    struct msghdr *msg;
    struct iovec msgvec[1];
    uint8_t broadcast_mac[6] = ETH_BROADCAST_ADDR;  /*Ethernet broadcast address*/

    /*Set up arp request message*/
    arp_request.type = MIP_ARP_REQUEST;
    arp_request.address = mip_address;
    arp_request.reserved = 0;

    /*Allocate the pdu*/
    pdu_request = alloc_pdu();

    uint8_t buffer[BUFFER_SIZE];

    /*For each interface send an arp request*/
    for (int i = 0; i < if_list->num_interfaces; i++) 
    {
        /*Make sure that buffer is set to 0*/
        memset(buffer, 0, sizeof(buffer));

        fill_pdu(
            pdu_request,                           /*Pointer to struct pdu*/
            if_list->interface_addrs[i].sll_addr,  /*Source mac address*/
            broadcast_mac,                         /*Destination mac address*/
            src_mip_address,                       /*Source mip address*/
            0xFF,                                  /*Destination mip address, in this case broadcast*/
            MIP_ARP,                               /*Sdu type mip arp message*/
            (uint8_t*)&arp_request,                /*Sdu, the arp request payload*/
            sizeof(struct mip_arp_message)         /*Size of the sdu*/
            );

        /*Serialize the pdu into byte stream*/
        size_t pdu_size = mip_serialize_pdu(pdu_request, buffer);
        /*Set appropriate values for sending*/
        msgvec[0].iov_base = buffer;
        msgvec[0].iov_len = pdu_size;

        msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

        msg->msg_name = &if_list->interface_addrs[i]; /*Correct interface*/
        msg->msg_namelen = sizeof(struct sockaddr_ll);
        msg->msg_iov = msgvec;
        msg->msg_iovlen = 1;

        if (sendmsg(raw_socket, msg, 0) == -1) /*Send the pdu over raw socket*/
        {
            perror("sendmsg");
        } else 
        {
            printf("Sent MIP-ARP PDU request for MIP address: %d on interface %d\n", mip_address, i);
            if(debug_mode)
            {
                print_pdu_content(pdu_request);
            }
        }
        
        free(msg);
    }
    /*Free allocated pdu after sending*/
    destroy_pdu(pdu_request);
}


void send_arp_response( int raw_socket, struct sockaddr_ll *so_name, uint8_t mip_address, uint8_t target_mip_address, 
                        struct interface_info *if_list, uint8_t dest_mac[6]) 
{
    struct mip_arp_message arp_response;
    struct pdu *pdu_response;
    struct msghdr *msg;
    struct iovec msgvec[1];

    /*Set up arp response message*/
    arp_response.type = MIP_ARP_RESPONSE;
    arp_response.address = mip_address;
    arp_response.reserved = 0;

    /*Allocate the pdu*/
    pdu_response = alloc_pdu();

    /*Find the interface that corresponds to the ifindex where the ARP request came from*/
    for (int i = 0; i < if_list->num_interfaces; i++) 
    {
        if (if_list->interface_addrs[i].sll_ifindex == so_name->sll_ifindex) /*If we find match*/
        {

            /*We add the correct entry to our cache*/
	        add_to_arp_cache(target_mip_address, dest_mac, if_list->interface_addrs[i].sll_addr);

            fill_pdu(pdu_response,                      /*Pointer to struct pdu*/
                if_list->interface_addrs[i].sll_addr,   /*Source mac address*/
		        dest_mac,                               /*Destination mac address*/
                mip_address,                            /*Source MIP address (our MIP address)*/
                target_mip_address,                     /*Destination MIP address (sender's MIP address)*/
                MIP_ARP,                                /*SDU Type (ARP response)*/
                (uint8_t*)&arp_response,                /*Sdu, the ARP response payload*/
                sizeof(struct mip_arp_message)          /*Size of the sdu*/
            );

            /*Make buffer and serialize the pdu into byte stream*/
            uint8_t buffer[BUFFER_SIZE];
            size_t pdu_size = mip_serialize_pdu(pdu_response, buffer);

            msgvec[0].iov_base = buffer;
            msgvec[0].iov_len = pdu_size;

            msg = (struct msghdr *)calloc(1, sizeof(struct msghdr)); /*Allocate memory for msg, and initialize as 0*/

            msg->msg_name = &if_list->interface_addrs[i];
            msg->msg_namelen = sizeof(struct sockaddr_ll);
            msg->msg_iov = msgvec;
            msg->msg_iovlen = 1;

            if (sendmsg(raw_socket, msg, 0) == -1) /*Send arp response over raw socket*/
            {
                perror("sendmsg");
                free(msg);
            } else 
            {
                printf("Sent MIP-ARP response: MIP address %d is at our MAC address\n", mip_address);
                if(debug_mode){
                    print_pdu_content(pdu_response);
                }
            }
            free(msg); /*Free msg*/
            break; /*If we find the matching interface we break the loop*/
        }
    }
    /*Free allocated pdu after sending*/
    destroy_pdu(pdu_response);
}
