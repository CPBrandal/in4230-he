#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h> // htons
#include "pdu.h"
#include "raw_socket.h"
#include "local_interfaces.h"
#include "ping.h"
#include "utils.h"

/*This file is inspired by the github repository we gained access to in learning, p4*/

struct pdu *alloc_pdu(void) 
{
    struct pdu *pdu = (struct pdu *)malloc(sizeof(struct pdu));

    if (pdu == NULL) 
    {
        perror("Failed to allocate memory for PDU");
        exit(EXIT_FAILURE);
    }

    /*Allocate memory for the Ethernet header and zero it*/
    pdu->ether_header = (struct ether_frame *)malloc(sizeof(struct ether_frame));
    if (pdu->ether_header == NULL) 
    {
        perror("Failed to allocate memory for Ethernet header");
        free(pdu);
        exit(EXIT_FAILURE);
    }
    pdu->ether_header->eth_proto = 0;

    /*Allocate memory for the MIP header*/
    pdu->mip_header = (struct mip_header *)malloc(sizeof(struct mip_header));
    if (pdu->mip_header == NULL) 
    {
        perror("Failed to allocate memory for MIP header");
        free(pdu->ether_header);
        free(pdu);
        exit(EXIT_FAILURE);
    }
    /*Set all fields to 0*/
    pdu->mip_header->dest_addr = 0;
    pdu->mip_header->src_addr = 0;
    pdu->mip_header->sdu_len = 0;
    pdu->mip_header->ttl = 0;
    pdu->mip_header->sdu_type = 0;

    pdu->sdu = NULL;
    
    return pdu;
}


void print_mac_addr(uint8_t *mac_addr, size_t length) 
{
    /*Safety check*/
    if (length != 6) 
    {
        printf("Invalid MAC address length\n");
        return;
    }

    /*Print the MAC address in hexadecimal format*/
    for (size_t i = 0; i < length; i++) 
    {
        printf("%02x", mac_addr[i]);
        if (i < length - 1) 
        {
            printf(":");
        }
    }
    printf("\n");
}


void print_pdu_content(struct pdu *pdu) 
{
    printf("---------------------\n");
    printf("SDU content print: \n");
    printf("The destination MAC address: \n");
	print_mac_addr(pdu->ether_header->dst_addr, 6);
	printf("The source MAC address: \n");
	print_mac_addr(pdu->ether_header->src_addr, 6);
	printf("Source MIP address: %u\n", pdu->mip_header->src_addr);
	printf("Destination MIP address: %u\n", pdu->mip_header->dest_addr);
	printf("SDU length: %d\n", pdu->mip_header->sdu_len * 4);
	printf("PDU type: 0x%02x\n", pdu->mip_header->sdu_type);
    if(pdu->mip_header->sdu_type == PING) /*Print the ping message*/
    {
	    printf("The SDU: %s\n", pdu->sdu);
    }
    printf("---------------------\n");
}


void fill_pdu(struct pdu *pdu,
              uint8_t *src_mac_addr,
              uint8_t *dst_mac_addr,
              uint8_t src_mip_addr,
              uint8_t dst_mip_addr,
              uint8_t type,
              uint8_t *sdu,
              uint8_t sdu_len_bytes) 
{

    /* Fill ethernet header */
    memcpy(pdu->ether_header->dst_addr, dst_mac_addr, 6);
    memcpy(pdu->ether_header->src_addr, src_mac_addr, 6);
    /* Fill mip header */
    pdu->mip_header->dest_addr = dst_mip_addr;
    pdu->mip_header->ttl = 1;
    pdu->mip_header->src_addr = src_mip_addr;
    pdu->mip_header->sdu_type = type;
    pdu->ether_header->eth_proto = htons(ETH_P_MIP);
    uint8_t length_sdu = sdu_len_bytes;

    /* Ensure the length is 32-bit aligned */
    if (length_sdu % 4 != 0) 
    {
        length_sdu += (4 - (length_sdu % 4));
    }

    /* Set the SDU length in terms of 32-bit words */
    pdu->mip_header->sdu_len = length_sdu / 4;
    if(pdu->sdu != NULL){
      free(pdu->sdu);
    }
    /* Allocate memory for the SDU length */
    pdu->sdu = (uint8_t *)calloc(1, length_sdu);
    if (pdu->sdu == NULL) 
    {
        perror("Failed to allocate memory for SDU");
        exit(EXIT_FAILURE);
    }

    /* Copy the actual SDU data */
    memcpy(pdu->sdu, sdu, sdu_len_bytes);
}


size_t mip_serialize_pdu(struct pdu *pdu, uint8_t *buffer) 
{
    size_t buffer_length = 0;

	/*Copy ethernet header*/
	memcpy(buffer + buffer_length, pdu->ether_header, sizeof(struct ether_frame));
	buffer_length += sizeof(struct ether_frame);

	/* Copy MIP header */
	uint32_t mip_header = 0;
    mip_header |= (uint32_t)pdu->mip_header->dest_addr << 24;
    mip_header |= (uint32_t)pdu->mip_header->src_addr << 16;
    mip_header |= (uint32_t)(pdu->mip_header->ttl & 0xF) << 12;
    mip_header |= (uint32_t)(pdu->mip_header->sdu_len & 0x1FF) << 3; 
    mip_header |= (uint32_t)(pdu->mip_header->sdu_type & 0x7); 

	/*Change it from host to network*/
	mip_header = htonl(mip_header);

	memcpy(buffer_length + buffer, &mip_header, sizeof(struct mip_header));
	buffer_length += sizeof(struct mip_header);

	/*Include the sdu*/
	memcpy(buffer_length + buffer, pdu->sdu, pdu->mip_header->sdu_len * 4);
	buffer_length += pdu->mip_header->sdu_len * 4;

    /*Return the length of the buffer*/
	return buffer_length;
}


size_t mip_deserialize_pdu(struct pdu *pdu, uint8_t *buffer) 
{
    size_t buffer_len = 0;

    /*Unpack the ether net header*/
    pdu->ether_header = (struct ether_frame *)malloc(sizeof(struct ether_frame));
    memcpy(pdu->ether_header, buffer + buffer_len, sizeof(struct ether_frame));
    buffer_len += sizeof(struct ether_frame);

    /*Unpack the mip header */
    pdu->mip_header = (struct mip_header*)malloc(sizeof(struct mip_header));
    uint32_t *tmp = (uint32_t*)(buffer + buffer_len);
    uint32_t header = ntohl(*tmp);  /*Convert from network to host order */

    pdu->mip_header->dest_addr = (uint8_t)(header >> 24);                   /*Extract the dest address (8 bits)*/ 
    pdu->mip_header->src_addr = (uint8_t)(header >> 16);                    /*Extract the src address (8 bits)*/
    pdu->mip_header->ttl = (uint8_t)((header >> 12) & 0xF);                 /*Extract the TTL(time to live) (4 bits)*/
    pdu->mip_header->sdu_len = (uint16_t)((header >> 3) & 0x1FF);           /*Extract the SDU length (9 bits)*/
    pdu->mip_header->sdu_type = (uint8_t)(header & 0x7);                    /*Extract the SDU type (3 bits)*/
    
    buffer_len += sizeof(struct mip_header);

    /*Unpack the SDU*/
    pdu->sdu = (uint8_t *)calloc(1, pdu->mip_header->sdu_len * 4);  /*Allocate memory for the SDU based on the length*/
    memcpy(pdu->sdu, buffer + buffer_len, pdu->mip_header->sdu_len * 4);
    buffer_len += pdu->mip_header->sdu_len * 4;

    /*Return the buffer length*/
    return buffer_len;
}


void send_pdu_to_raw_socket(int raw_socket, struct pdu *send_pdu, struct interface_info *if_list) 
{
    /*Prepare buffer for sending*/
    uint8_t buffer[BUFFER_SIZE];
    size_t pdu_size = mip_serialize_pdu(send_pdu, buffer);
    struct msghdr *msg;
    struct iovec msgvec[1];

    /*Find the interface based on the mac address*/
    struct sockaddr_ll *dest = find_interface_by_mac(if_list, send_pdu->ether_header->src_addr);
    
    if (dest == NULL) 
    {
        printf("Error: Could not find MAC in the interface list\n");
        return;
    }

    /*Prepare for sending*/
    msgvec[0].iov_base = buffer;
    msgvec[0].iov_len = pdu_size;

    msg = (struct msghdr *)calloc(1, sizeof(struct msghdr));

    msg->msg_name = dest;
    msg->msg_namelen = sizeof(struct sockaddr_ll);
    msg->msg_iov = msgvec;
    msg->msg_iovlen = 1;

    if(debug_mode)
    {
        print_pdu_content(send_pdu);
    }

    /*Send the pdu over raw socket*/
    if (sendmsg(raw_socket, msg, 0) == -1) 
    {
        perror("sendmsg");
    } else 
    {
        printf("PDU sent over raw socket\n");
    }
    free(msg);
}


void destroy_pdu(struct pdu *pdu) 
{
  if(pdu == NULL){
    return;
  }
  if(pdu->ether_header != NULL){
    free(pdu->ether_header);
  }
  
  if(pdu->mip_header != NULL){
    free(pdu->mip_header);
  }
  
  if(pdu->sdu != NULL){
    free(pdu->sdu);
  }
  free(pdu);
}
