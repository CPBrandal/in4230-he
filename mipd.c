#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <stdint.h>
#include "raw_socket.h"  // Include raw socket header for our functions
#include "mip_arp.h"
#include "local_interfaces.h"
#include "pdu.h"
#include "ping.h"
#include "utils.h" /*print_help & create_unix_socket*/

/*Define max events on our epoll, I assume we do not need to many, however this can easily be changed here.*/
#define MAX_EVENTS 20

int main(int argc, char *argv[]) 
{
    /*Prepare values*/
    int first = 0; /*Value for calling get_local_interfaces*/
    int raw_socket, unix_socket, connection_socket = -1; /*Sockets*/
    char *socket_upper = NULL; /*Upper socket, given from command line*/
    int mip_address = 0;
    memset(arp_list, 0, sizeof(arp_list));
    int rc;

    struct interface_info if_list; /*Struct to hold our interfaces*/

    /*Check arguments*/
    int opt;
    while ((opt = getopt(argc, argv, "hd")) != -1) 
    {
        switch (opt) 
        {
            case 'h': /*Case where user wants help*/
                print_help("Usage: mipd [-h] [-d] <socket_upper> <MIP address>");
                exit(EXIT_SUCCESS);
            case 'd': /*Case where user wants debug mode*/
                debug_mode = 1;
                break;
            default: /*Case where user did something wrong*/
                print_help("Usage: mipd [-h] [-d] <socket_upper> <MIP address>");
                exit(EXIT_FAILURE);
        }
    }

    /*Ensure only two arguments exist*/
    if (optind + 2 != argc) 
    {
        fprintf(stderr, "Error: Missing required arguments.\n");
        print_help("Usage: mipd [-h] [-d] <socket_upper> <MIP address>");
        exit(EXIT_FAILURE);
    }

    /*Collect appropriate data, upper_socket and mip address*/
    socket_upper = argv[optind];
    mip_address = atoi(argv[optind + 1]);

    /* Debug mode output when enabled*/
    if (debug_mode) 
    {
        printf("Debug mode enabled\n");
        printf("UNIX socket path: %s\n", socket_upper);
        printf("MIP address: %d\n", mip_address);
    }

    /*Initialize the empty ARP cache*/
    initialize_arp_cache();

    /*Create UNIX- and raw sockets*/
    unix_socket = create_unix_socket(socket_upper);
    raw_socket = create_raw_socket();

    /*Create epoll*/
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) 
    {
        perror("epoll_create1");
        close(unix_socket);
        close(raw_socket);
        return -1;
    }

    struct epoll_event ev, events[MAX_EVENTS];

    /*Add UNIX socket to epoll*/
    ev.events = EPOLLIN;
    ev.data.fd = unix_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, unix_socket, &ev) == -1) 
    {
        perror("epoll_ctl: unix_socket");
        close(unix_socket);
        close(raw_socket);
        return -1;
    }

    /*Add raw socket to epoll*/
    ev.events = EPOLLIN;
    ev.data.fd = raw_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, raw_socket, &ev) == -1) 
    {
        perror("epoll_ctl: raw_socket");
        close(unix_socket);
        close(raw_socket);
        return -1;
    }

    uint8_t buf[BUFFER_SIZE];

    while (1) 
    {
        rc = epoll_wait(epoll_fd, events, MAX_EVENTS, -1); /*Wait for incoming traffic*/
        if (rc == -1) 
        {
            perror("epoll_wait");
            break;
        }
        if(first == 0)
        {
            /*I assume that the user creates all nodes/hosts first then call .ping_client, therefore we get interfaces after we have received a message once.*/
            get_local_interfaces(&if_list, raw_socket);
            first = 1;
        }
        if (events->data.fd == unix_socket) /*Handle connection message from unix socket*/
        {
            struct sockaddr_un client_addr;
            socklen_t client_addr_len = sizeof(client_addr);
            connection_socket = accept(unix_socket, (struct sockaddr *)&client_addr, &client_addr_len);
            
            if (connection_socket == -1) /*Error handleing*/
            {
                perror("accept");
                continue;
            }

            /*Add the new connection to the epoll table*/
            ev.events = EPOLLIN;
            ev.data.fd = connection_socket;
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connection_socket, &ev) == -1)
            {
                perror("epoll_ctl: connection_socket");
                close(connection_socket);
                continue;
            }
            if(debug_mode)
            {
                printf("Accepted new connection on UNIX socket.\n");
            }
        } else if (events->data.fd == connection_socket) /*Handle message from application*/
        {
            memset(buf, 0, BUFFER_SIZE);
            rc = recv(connection_socket, buf, BUFFER_SIZE, 0);
            
            if (rc > 0) /*Recv was a success, handleing incomming message*/
            {
                /*Deserialize ping message*/
                struct ping_message *received_ping = (struct ping_message *)malloc(sizeof(struct ping_message));
                if (deserialize_ping_message(received_ping, buf, rc) == 1) 
                {
                    if(debug_mode)
                    {
                        printf("Received ping_message from UNIX connection socket:\nMIP Address: %u\nMessage: %s\n", 
                            received_ping->mip_address, received_ping->msg);
                    }
                    /*Store the ping message in global variable*/
                    store_ping_message(received_ping);
                    
                    /*Lookup the destination mac address*/
                    uint8_t* mac_ad = lookup_mac_dest(received_ping->mip_address);
		    

                    if (mac_ad == NULL) /*If we dont find a mac, we have to send arp request*/
                    {
                        if(debug_mode){
                            printf("Can not find mac destination, sending arp request.\n");
                        }
                        send_arp_request(raw_socket, &if_list, received_ping->mip_address, mip_address);
			free(received_ping);
                    } else /*We found the correct mac address*/
                    {
                        /**/
                        uint8_t* mac_src = lookup_mac_src(received_ping->mip_address);
                        if (mac_src == NULL) /*If we cant find the source address, then we print error*/
                        {
                            printf("Can not find mac source, can not send pdu.\n");
			    free(received_ping);
                        } else 
                        {
                            if(debug_mode)
                            {
                                print_ping_message(received_ping);
                            }
                            /*Prepare buffer*/
                            uint8_t ping_buffer[sizeof(struct ping_message)];
                            size_t ping_buffer_len = sizeof(ping_buffer);

                            if(!serialize_ping_message(stored_ping, ping_buffer, &ping_buffer_len)) 
                            {
                                printf("Failed to serialize message");
				free(received_ping);
                            } else 
                            {
                                /*Allocate and fill pdu*/
                                struct pdu *send_pdu = alloc_pdu();
                        
                                fill_pdu(send_pdu, 
                                        mac_src,                   // Source MAC
                                        mac_ad,                    // Destination MAC
                                        mip_address,               // Source MIP address
                                        received_ping->mip_address, // Destination MIP address
                                        PING,                      // SDU type
                                        ping_buffer,  // SDU content
                                        ping_buffer_len);    // SDU size
                                /*Send pdu over raw socket*/
                                send_pdu_to_raw_socket(raw_socket, send_pdu, &if_list);
				destroy_pdu(send_pdu);
				free(received_ping);
                            }
                        }
                    }
                } else /*Deserialize fail*/
                {
                    printf("Failed to deserialize the ping_message.\n");
		    free(received_ping);
                }
            } else if(rc == 0) /*The connection to the application has been closed*/
            {
                printf("Application has closed its connection\n");

                if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection_socket, NULL) == -1) /*Remove the connection from the epoll table*/
                {
                    perror("epoll_ctl: EPOLL_CTL_DEL");
                }
                printf("Removed connection from epoll.\n");
                close(connection_socket); /*Close socket*/
                connection_socket = -1;
            } else /*Error in receiving from the applucation*/
            {
                perror("recv: connection_socket"); 

                if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, connection_socket, NULL) == -1) /*Remove the connection from the epoll table*/
                {
                    perror("epoll_ctl: EPOLL_CTL_DEL");
                }
                printf("Removed connection from epoll.\n");
                close(connection_socket); /*Close socket*/
                connection_socket = -1;
            }
        } else if (events->data.fd == raw_socket) /*Handle message from raw socket*/
        {
            handle_received_pdu(raw_socket, &if_list, mip_address, connection_socket);
        }
        
    }

    /*Free stored memory*/
    free(stored_ping);
    close(unix_socket);
    close(raw_socket);
    return 0;
}

