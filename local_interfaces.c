#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include "local_interfaces.h"
#include "utils.h"

void get_local_interfaces(struct interface_info *if_list, int socket_fd) 
{
    struct ifaddrs *ifaces, *iface;
    if_list->num_interfaces = 0; 

    /*Get the list of network interfaces*/
    if (getifaddrs(&ifaces) == -1) 
    {
        perror("getifaddrs");
        exit(EXIT_FAILURE);
    }

    /*Loop through all iterations and add the interfaces to the list*/
    for (iface = ifaces; iface != NULL; iface = iface->ifa_next) 
    {
        if (iface->ifa_addr != NULL && iface->ifa_addr->sa_family == AF_PACKET) 
        {
            struct sockaddr_ll *sll = (struct sockaddr_ll *)iface->ifa_addr;
            if (strcmp(iface->ifa_name, "lo") != 0)  /*Avoid the loopback interfaces*/
            {
                /*We copy the interface structure to our list*/
                memcpy(&if_list->interface_addrs[if_list->num_interfaces], sll, sizeof(struct sockaddr_ll));

                /*Increase number of interfaces*/
                if_list->num_interfaces++;

                if(debug_mode)
                {
                    char mac_str[18];
                    snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                            sll->sll_addr[0], sll->sll_addr[1], sll->sll_addr[2],
                            sll->sll_addr[3], sll->sll_addr[4], sll->sll_addr[5]);
                    printf("Interface %s, MAC: %s, ifindex: %d\n", iface->ifa_name, mac_str, sll->sll_ifindex);
                }

                if (if_list->num_interfaces >= MAX_INTERFACES) /*If we reach the maximum number of interfaces we stop*/
                {
                    break;
                }
            }
        }
    }

    /*Store the raw socket file descriptor*/
    if_list->socket_fd = socket_fd;
    /*Lastly we free the ifaddrs*/
    freeifaddrs(ifaces);
}


uint8_t* find_mac_address(struct interface_info *if_list, struct sockaddr_ll *so_name) 
{
    if(if_list == NULL || so_name == NULL) /*Safety check*/
    {
        return NULL;
    }

    for (int i = 0; i < if_list->num_interfaces; i++) /*We loop through all interfaces in the if_list*/
    {

        if (if_list->interface_addrs[i].sll_ifindex == so_name->sll_ifindex) /*Compare the MAC address of the current interface with the given mac address*/
        {
            /*Return mac address of the given interface */
            return if_list->interface_addrs[i].sll_addr;
        }
    }
    /*Return null if we dont find a match*/
    return NULL;
}


struct sockaddr_ll* find_interface_by_mac(struct interface_info *if_list, uint8_t *mac_addr) 
{
    if (if_list == NULL || mac_addr == NULL) /*Safety check*/
    {
        return NULL;
    }

    for (int i = 0; i < if_list->num_interfaces; ++i) /*We loop through all interfaces in the if_list*/
    {
        if (memcmp(if_list->interface_addrs[i].sll_addr, mac_addr, 6) == 0) /*Compare the MAC address of the current interface with the given mac address*/
        {
            /* mac address matches, return the corresponding interface*/
            return &if_list->interface_addrs[i];
        }
    }

    /*Return null if we dont find a match*/
    return NULL;
}