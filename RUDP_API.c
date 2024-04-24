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
#include <sys/time.h>

// defines
#define Max_data_size 5 * 1024 * 1024
// timeout value for use. in seconds
#define RUDP_TIMEOUT_DEFUALT 3
//

unsigned short int calculate_checksum(void *data, unsigned int bytes);
void socket_settings(RUDP_Socket *sock);
RudpPacket *packet_create(int is_data, int ack, int syn, int fin, char *data, unsigned int length);
RudpPacket *packet_recive();
int packet_send(RUDP_Socket *sock_fd, RudpPacket *pack);

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
        socket_settings(sock);
        if (isServer)
        {

            // The variable to store the server's address.
            struct sockaddr_in server;
            memset(&server, 0, sizeof(server));

            // Set the server's address to "0.0.0.0" (all IP addresses on the local machine).
            server.sin_addr.s_addr = INADDR_ANY;

            // Set the server's address family to AF_INET (IPv4).
            server.sin_family = AF_INET;
            // bind socket to port
            server.sin_port = htons(listen_port);

            int bindstat = -1;
            bindstat = bind(sock->socket_fd, (struct sockaddr *)&server, sizeof(server));
            if (bindstat < 0)
            {
                perror("rudp_socket_setsockopt");
                rudp_close(sock);
                return NULL;
            }
        }
        sock->isConnected = false;
    }

    return sock;
}

int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port)
{
    char *ack_buffer[RUDP_HEADER_SIZE];
    if (sockfd->isConnected == true || sockfd->isServer == true)
        return 0;

    sockfd->dest_addr->sin_family = AF_INET;
    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, dest_ip, &sockfd->dest_addr->sin_addr) <= 0)
    {
        perror("rudp_connect_inet_pton(3)");
        close(rudp_close(sockfd));
        return 0;
    }

    sockfd->dest_addr->sin_port = htons(dest_port);

    if (connect(sockfd->socket_fd, sockfd->dest_addr, sizeof(sockfd->dest_addr)) < 0)
    {
        perror("rudp_connect-connect: ");
        return 0;
    }
    // receive ack from connected
    if (rudp_recv(sockfd, ack_buffer, RUDP_HEADER_SIZE) < 0)
    {
        perror("rudp_connect-ACK: ");
        return 0;
    }
    sockfd->isConnected = true;
    return 1;
}

int rudp_accept(RUDP_Socket *sockfd)
{
    if (!(sockfd->isServer))
    {
        perror("rudp_accept: socket is set to client.");
        return 0;
    }
    if (sockfd->isConnected)
    {
        perror("rudp_accept: socket is already connected. ");
        return 0;
    }

    char *ack_buffer[RUDP_HEADER_SIZE];

    int client_sock = accept(sockfd->socket_fd, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));

    if (client_sock < 0)
    {
        perror("rudp_accept: ");
        return 0;
    }
    int ack_sent = -1;
    // ack_sent = sendto(client_sock, RUDP_SYNACK, sizeof(RUDP_SYNACK), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
    if (ack_sent < -1)
    {
        perror("rudp_accept_ACK: ");
        return 0;
    }
    return 1;
}

int rudp_recv(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    int bytes_received = recvfrom(sockfd->socket_fd, buffer, buffer_size, 0, NULL, NULL);
    if (bytes_received == -1)
    {
        perror("Error while receiving data");
        return -1;
    }

    return bytes_received;
}

int rudp_send(RUDP_Socket *sockfd, void *buffer, unsigned int buffer_size)
{
    int numofpackets = buffer_size / Max_window_size;
    int lastpacket = buffer_size % Max_window_size;
    for (int i = 0; i < numofpackets; i++)
    {
        RudpPacket *pck = packet_create(1, 0, 0, 0, buffer, Max_window_size);
        int res = packet_send(sockfd, pck);
    }

    return 0;
}

int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!(sockfd->isConnected))
    {
        return 0;
    }

    return 0;
}

int rudp_close(RUDP_Socket *sockfd)
{
    if (sockfd != NULL)
    {
        if (sockfd->dest_addr != NULL)
        {
            free(sockfd->dest_addr);
        }
        free(sockfd);
    }
    return 0;
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

void socket_settings(RUDP_Socket *sock)
{
    int opt = 1;
    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("rudp__setsockopt:reuse address");
        rudp_close(sock);
        return NULL;
    }

    struct timeval to;
    to.tv_sec = RUDP_TIMEOUT_DEFUALT;
    to.tv_usec = 0;

    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0)
    {
        perror("rudp_setsockopt:timeout");
        rudp_close(sock);
        return NULL;
    }
}

// create a packet with flags and copy length from data
RudpPacket *packet_create(int is_data, int ack, int syn, int fin, char *data, unsigned int length)
{
    RudpPacket *pack = malloc(sizeof(RudpPacket));
    memset(pack, 0, sizeof(RudpPacket));
    pack->flags.data = is_data;
    pack->flags.ack = ack;
    pack->flags.syn = syn;
    pack->flags.fin = fin;
    if (is_data)
    {
        unsigned short int check = calculate_checksum(data, length);
        memcpy(pack->data, data, length);
        pack->length = length;
        pack->checksum = check;
    }
    return pack;
}

//return 1
int packet_send(RUDP_Socket *sock_fd, RudpPacket *pack)
{
    // send packet
    int sent = 0;
    sent = sendto(sock_fd->socket_fd, pack, sizeof(pack), 0, NULL, 0);
    if (sent < 0)
    {
        perror("packet_send-send: ");
        return 0;
    }

    // recieve ack
    RudpPacket *reply = (RudpPacket *)malloc(sizeof(RudpPacket));
    int bytesreplyed = recvfrom(sock_fd->socket_fd,reply,Max_window_size,0,NULL,0);
    if (bytesreplyed < 0)
    {
        perror("packet_send-recive ack: ");
        free(reply);
        return 0;
    }
    if(reply->length == pack->length && reply->flags.ack==1){
        free(reply);
        return 1;
    }
    free(reply);
    return 0;
}
