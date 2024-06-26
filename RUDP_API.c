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
#include <unistd.h>
// timeout value for use. in seconds
#define RUDP_TIMEOUT_DEFUALT 3
// how many retrensmissions a packet makes before returning an error
#define RUDP_MAX_RETRANSMISSIONS 2
// buffer data that symbolize a full package of data was received
#define PACKAGE_DELIVER_END "fnsh"
// #define DEBUG 1
unsigned short int calculate_checksum(void *data, unsigned int bytes);
void socket_settings(RUDP_Socket *sock);
void set_recv_timer(RUDP_Socket *sock, int timeinsec);
RudpPacket *packet_create(int is_data, int ack, int syn, int fin, char *data, unsigned int size, unsigned int seq);
RudpPacket *packet_recieve(RUDP_Socket *sock_fd, unsigned int seq);
int packet_send(RUDP_Socket *sock_fd, RudpPacket *pack);
int recive_ack(RUDP_Socket *sock_fd, RudpPacket *pack);
int delivery_finish_send(RUDP_Socket *sock, unsigned int seq);
void set_send_timer(RUDP_Socket *sock, int timeinsec);

RUDP_Socket *rudp_socket(bool isServer, int listen_port)
{
    RUDP_Socket *sock = (RUDP_Socket *)malloc(sizeof(RUDP_Socket));
    if (sock != NULL)
    {
        // sock->dest_addr = (struct sockaddr_in *)malloc(sizeof(struct sockaddr_in));
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        sock->socket_fd = sock_fd;
        sock->isServer = isServer;
        sock->dest_addr = NULL;
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
                perror("rudp_socket_bind");
                // rudp_close(sock);
                return NULL;
            }
        }
        sock->isConnected = false;
    }

    return sock;
}

int rudp_connect(RUDP_Socket *sockfd, const char *dest_ip, unsigned short int dest_port)
{
    RudpPacket *rep = (RudpPacket *)malloc(sizeof(RudpPacket));

    if (sockfd->isConnected == true || sockfd->isServer == true)
        return 0;
    set_recv_timer(sockfd, RUDP_TIMEOUT_DEFUALT * 3);
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, dest_ip, &dest.sin_addr) <= 0)
    {
        perror("rudp_connect_inet_pton(3)");
        // rudp_close(sockfd);
        return 0;
    }

    dest.sin_port = htons(dest_port);
    // debug_print_socket(sockfd);
    if (connect(sockfd->socket_fd, (struct sockaddr *)&dest, sizeof(dest)) < 0)
    {
        perror("rudp_connect-connect: ");
        return 0;
    }
    sockfd->dest_addr = &dest;
    RudpPacket *synpack = packet_create(0, 0, 1, 0, NULL, 0, 0);

    int sent = sendto(sockfd->socket_fd, synpack, sizeof(synpack), 0, NULL, 0);
    if (sent < 0)
    {
        perror("rudp_connect-send syn: ");
        return 0;
    }
    // receive ack from connected
    int n = recvfrom(sockfd->socket_fd, rep, RUDP_HEADER_SIZE, 0, (struct sockaddr *)NULL, NULL);
    if (n < 0)
    {
        perror("rudp_connect-ACK: ");
        return 0;
    }
    set_recv_timer(sockfd, RUDP_TIMEOUT_DEFUALT);
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
    RudpPacket *synpack = (RudpPacket *)malloc((sizeof(RudpPacket)));
    memset(synpack, 0, sizeof(RudpPacket));

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));

    socklen_t len = sizeof(dest);
    int n = recvfrom(sockfd->socket_fd, synpack, sizeof(synpack), 0, (struct sockaddr *)&dest, &len);
    if (n < 0)
    {
        perror("rudp_accept: ");
        return 0;
    }

    // debug_print_socket(sockfd);
    if (connect(sockfd->socket_fd, (struct sockaddr *)&dest, sizeof(dest)) < 0)
    {
        perror("rudp_accept-connect: ");
        return 0;
    }
    sockfd->dest_addr = &dest;

    int ack_sent = -1;
    // ack_sent = sendto(client_sock, RUDP_SYNACK, sizeof(RUDP_SYNACK), 0, (struct sockaddr *)&(sockfd->dest_addr), sizeof(sockfd->dest_addr));
    RudpPacket *ackpack = packet_create(0, 0, 1, 0, NULL, 0, 0);
    ack_sent = sendto(sockfd->socket_fd, ackpack, sizeof(ackpack), 0, NULL, 0);
    if (ack_sent < -1)
    {
        perror("rudp_accept_ACK: ");
        return 0;
    }
    free(ackpack);
    ackpack = NULL;
    free(synpack);
    synpack = NULL;
    set_send_timer(sockfd, RUDP_TIMEOUT_DEFUALT);
    // set_recv_timer(sockfd, RUDP_TIMEOUT_DEFUALT*3);
    sockfd->isConnected = true;
    return 1;
}

int rudp_recv(RUDP_Socket *sockfd, char *buffer, int buffer_size)
{
    static unsigned int sequence = 0;
    bool is_continue = 1;
    int bytecounter = 0;

    if (sockfd->isConnected == false)
    {
        return -1;
    }

    RudpPacket *pack;
    // printf("clearing buffer\n");
    buffer[0] = 0;
    // strcpy(buffer,"");

    do
    {
        pack = packet_recieve(sockfd, sequence);
        // printf("packet data recived is %s",pack->data);
        if (pack != NULL)
        {
            if (pack->flags.data == true)
            {
                if (strcmp(pack->data, PACKAGE_DELIVER_END) == 0)
                {
                    is_continue = 0;
                }
                else
                {
                    int len = strlen(pack->data);
                    if (buffer_size > bytecounter + len)
                    {
                        strncat(buffer, pack->data, len);
                        bytecounter += len;
                        sequence++;
                    }
                }
            }
            else if (pack->flags.fin == 1)
            {
                is_continue = 0;
            }
        }
        else
        {
            is_continue = 0;
            bytecounter = -1;
        }
    } while (is_continue);

    return bytecounter;
}

int rudp_send(RUDP_Socket *sockfd, char *buffer, int buffer_size)
{
    int numofpackets;
    int lastpacketsize;
    char temp[Window_size] = {'\n'};
    if (buffer_size < Window_size)
    {
        numofpackets = 0;
        lastpacketsize = buffer_size;
    }
    else
    {
        numofpackets = buffer_size / (int)Window_size;
        lastpacketsize = buffer_size % (int)Window_size;
    }
    RudpPacket *pck;
    for (int i = 0; i < numofpackets; i++)
    {
        memcpy(temp, (buffer + i * Window_size), Window_size);
        pck = packet_create(1, 0, 0, 0, temp, Window_size, i);
        int res = packet_send(sockfd, pck);
        if (res == -1)
        {
            perror("err_packet send: ");
            free(pck);
            return -1;
        }
        else if (res == 0)
        {
            free(pck);
            return 0;
        }
        free(pck);
    }
    if (lastpacketsize > 0)
    {
        memcpy(temp, (buffer + numofpackets * Window_size), lastpacketsize);
        pck = packet_create(1, 0, 0, 0, temp, lastpacketsize, numofpackets);
        int res = packet_send(sockfd, pck);
        if (res == -1)
        {
            perror("err_last packet send: ");
            free(pck);
            pck = NULL;
            return -1;
        }
        else if (res == 0)
        {
            free(pck);
            pck = NULL;
            return 0;
        }
        free(pck);
        pck = NULL;
    }
    int fnshres = delivery_finish_send(sockfd, numofpackets + 1);
    if (fnshres == -1)
    {
        perror("finish packet send: ");
        free(pck);
        pck = NULL;
        return -1;
    }
    else if (fnshres == 0)
    {
        free(pck);
        pck = NULL;
        return 0;
    }

    return buffer_size;
}

int rudp_disconnect(RUDP_Socket *sockfd)
{
    if (!(sockfd->isConnected))
    {
        return 0;
    }
    RudpPacket *pck = packet_create(0, 0, 0, 1, NULL, 0, 0);
    int res = packet_send(sockfd, pck);
    if (res == 0)
    {
        sockfd->isConnected = false;
        if (sockfd->dest_addr != NULL)
        {
            free(sockfd->dest_addr);
            sockfd->dest_addr = NULL;
        }
    }
    return 1;
}

int rudp_close(RUDP_Socket *sockfd)
{
    if (sockfd != NULL)
    {
        close(sockfd->socket_fd);
        free(sockfd);
        sockfd = NULL;
    }
    return 1;
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

// sets settings (exluding timeouts) for sock (setsockopt use).
void socket_settings(RUDP_Socket *sock)
{
    int opt = 1;
    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("rudp__setsockopt:reuse address");
        rudp_close(sock);
    }
}
// sets setting of recv timeout of timeinsec seconds for sock (setsockopt use).
void set_recv_timer(RUDP_Socket *sock, int timeinsec)
{
    struct timeval to;
    to.tv_sec = timeinsec;
    to.tv_usec = 0;

    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_RCVTIMEO, &to, sizeof(to)) < 0)
    {
        perror("rudp_setsockopt:timeout");
        rudp_close(sock);
        // return NULL;
    }
}

// sets setting of send timeout of timeinsec seconds for sock (setsockopt use).
void set_send_timer(RUDP_Socket *sock, int timeinsec)
{
    struct timeval to;
    to.tv_sec = timeinsec;
    to.tv_usec = 0;

    if (setsockopt(sock->socket_fd, SOL_SOCKET, SO_SNDTIMEO, &to, sizeof(to)) < 0)
    {
        perror("rudp_setsockopt:timeout send");
        rudp_close(sock);
        // return NULL;
    }
}

// create a packet with flags and copy length from data
RudpPacket *packet_create(int is_data, int ack, int syn, int fin, char *data, unsigned int size, unsigned int seq)
{
    RudpPacket *pack = malloc(sizeof(RudpPacket));
    pack->flags.data = is_data;
    pack->flags.ack = ack;
    pack->flags.syn = syn;
    pack->flags.fin = fin;
    unsigned short int check;
    if (is_data && size > 0)
    {
        memcpy(pack->data, data, size);
        pack->length = seq;
    }
    check = calculate_checksum(pack->data, Window_size);
    pack->checksum = check;
    return pack;
}

// returns packet if good else returns NULL
RudpPacket *packet_recieve(RUDP_Socket *sock_fd, unsigned int seq)
{

    RudpPacket *pck = (RudpPacket *)malloc(sizeof(RudpPacket));
    memset(pck, 0, sizeof(RudpPacket));
    int res = recvfrom(sock_fd->socket_fd, pck, sizeof(RudpPacket) - 1, 0, NULL, 0);
    // recvfrom('',buffer)       buffer char[windowsize]
    if (res < 0)
    {
        perror("packet_recive-recv: ");
        free(pck);
        pck = NULL;
        return NULL;
    }
    unsigned short int recievedchecksum = pck->checksum;
    unsigned short int currcheck;
    currcheck = calculate_checksum(pck->data, Window_size);

    printf("pakcet_receive: received pack with size:%d\n", res);
    // rep = replay packet
    RudpPacket *rep = (RudpPacket *)malloc(sizeof(RudpPacket));
    memset(rep, 0, sizeof(RudpPacket));
    if (pck->flags.syn == 1)
    {
        rep->flags.ack = 1;
        rep->flags.syn = 1;
        rep->length = 0;
        rep->checksum = currcheck;
    }
    else if (pck->flags.fin == 1)
    {
        rep->flags.ack = 1;
        rep->flags.fin = 1;
        rep->length = -1;
        rep->checksum = currcheck;
    }
    else
    {
        if (recievedchecksum != currcheck)
        {
            rep->flags.ack = 0;
            rep->length = seq;
            rep->checksum = currcheck;
            free(pck);
            pck = NULL;
        }
        else
        {
            rep->flags.ack = 1;
            rep->length = pck->length;
            rep->checksum = currcheck;
        }
    }

    rep->flags.data = 0;
    int t = sendto(sock_fd->socket_fd, rep, sizeof(RudpPacket), 0, NULL, 0);

    printf("pakcet_receive: sent reply with size:%d\n", t);
    free(rep);
    rep = NULL;

    return pck;
}

// return 1 for succses -1 for fail and 0 for fin recv
int packet_send(RUDP_Socket *sock_fd, RudpPacket *pack)
{
    int retransmissions_count = RUDP_MAX_RETRANSMISSIONS + 1;
    RudpPacket *reply = (RudpPacket *)malloc(sizeof(RudpPacket));
    do
    {
        retransmissions_count--;
        // send packet
        int sent = 0;

        sent = sendto(sock_fd->socket_fd, pack, Max_window_size, 0, NULL, 0);
        if (sent < 0)
        {
            perror("packet_send-send: \n");
        }
        else
        {
            // recieve ack
            // printf("packet recieve reply\n");

            int bytesreplyed = recvfrom(sock_fd->socket_fd, reply, sizeof(RudpPacket) - 1, 0, NULL, 0);
            if (bytesreplyed < 0)
            {
                perror("packet_send-recive ack: ");
                // free(reply);
                // reply = NULL;
                // return -1;
            }
            else
            {
                if (reply->flags.ack == 1)
                {

                    free(reply);
                    reply = NULL;
                    return 1;
                }
                if (reply->flags.fin == 1)
                {
                    free(reply);
                    reply = NULL;
                    return 0;
                }
            }
        }

    } while (retransmissions_count > 0);

    free(reply);
    reply = NULL;
    return -1;
}

/*
tells the reciever that one full package has finished sending.
returns 1=succses, 0=fin, -1=error
*/
int delivery_finish_send(RUDP_Socket *sock, unsigned int seq)
{
    RudpPacket *pck = packet_create(1, 0, 0, 0, "fnsh", 5, seq);
    int res = packet_send(sock, pck);
    if (res == -1)
    {
        // perror("last packet send: ");
        free(pck);
        pck = NULL;
        return -1;
    }
    else if (res == 0)
    {
        free(pck);
        pck = NULL;
        return 0;
    }
    return 1;
}