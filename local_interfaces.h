#ifndef INTERFACE_INFO_H
#define INTERFACE_INFO_H

#include <stdint.h>
#include <sys/socket.h>
#include <linux/if_packet.h>

/*A max cap of numbers of interfaces, one for each mipd, and we can have 253 other mipds*/
#define MAX_INTERFACES 253


/*Struct for containing the interfaces of the mipd, contains a list of sockaddr_ll, the raw socket fd, and number of interfaces */
struct interface_info {
    struct sockaddr_ll interface_addrs[MAX_INTERFACES];
    int socket_fd;
    int num_interfaces;
};


/*Function to find the mac address of a given interface.
Function takes a pointer to struct interface_info and a pointer to a specific interface as parameters.
Function returns the mac address of the correct interface, or NULL if none is found.*/
uint8_t* find_mac_address(struct interface_info *if_list, struct sockaddr_ll *so_name);


/*Function to iterate through the network interfaces which are directly connected and add them to the list in interface_info.
The function also updates the count, as well as to set the socket file descriptor.
Function takes a pointer to a struct interface_info and a raw socket descriptor.*/
void get_local_interfaces(struct interface_info *if_list, int socket_fd);


/*This function finds the local interface in a given struct interface_info based on a given mac address
The function takes a pointer to struct interface_info and a mac address as parameters.
The function either returns the correct struct sockaddr_ll (interface) or NULL*/
struct sockaddr_ll* find_interface_by_mac(struct interface_info *if_list, uint8_t *mac_addr);

#endif
