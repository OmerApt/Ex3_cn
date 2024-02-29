#include "RUDP_API.h"
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdbool.h>

unsigned short int calculate_checksum(void *data, unsigned int bytes);

RUDP_Socket *rudp_socket(bool isServer, unsigned short int listen_port)
{
    RUDP_Socket *sock = malloc(sizeof(RUDP_Socket));
    if (sock != NULL)
    {
        memset(sock->dest_addr, 0, sizeof(sock->dest_addr));
        if (sock->dest_addr == NULL)
        {
            perror("sock.destaddress");
        }
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        sock->socket_fd = sock_fd;
        sock->isServer = isServer;
        if (isServer)
        {
            // bind socket to port
            sock->dest_addr->sin_port = listen_port;
        }
        sock->isConnected = false;
    }

    return sock;
}

int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port)
{
    if (sockfd->isConnected == true)
        return 0;

    sockfd->dest_addr->sin_family = AF_INET;
    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, dest_ip, &sockfd->dest_addr->sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        close(rudp_close(sockfd));
        return 0;
    }

    sockfd->dest_addr->sin_port = htons(dest_port);

    if (connect(sockfd->socket_fd, sockfd->dest_addr, sizeof(sockfd->dest_addr))<0){
        return 0;
        perror("connect: ");
    }
        return 1;
}

int rudp_accept(RUDP_Socket *sockfd)
{
    return 0;
}

int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    return 0;
}

int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    return 0;
}

int rudp_disconnect(RUDP_Socket *sockfd)
{
    return 0;
}

int rudp_close(RUDP_Socket *sockfd)
{
    return 0;
}

void rudp_alloc()
{
}

/*
* @brief
A checksum function that returns 16 bit checksum for data.
* @param data
The data to do the checksum for.
* @param bytes
The length of the data in bytes.
* @return
The checksum itself as 16 bit unsigned number.
* @note
This function is taken from RFC1071, can be found here:
* @note
https://tools.ietf.org/html/rfc1071
* @note
It is the simplest way to calculate a checksum and is not very strong.
*
However, it is good enough for this assignment.
* @note
You are free to use any other checksum function as well.
*
You can also use this function as such without any change.
*/
unsigned short int calculate_checksum(void *data, unsigned int bytes)
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;
    // Main summing loop
    while (bytes > 1)
    {
        total_sum += *data_pointer++;
        bytes -= 2;
    }
    // Add left-over byte, if any
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);
    // Fold 32-bit sum to 16 bits
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    return (~((unsigned short int)total_sum));
}