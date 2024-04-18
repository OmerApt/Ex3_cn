// Includes
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include "RUDP_API.h"


// define
#define DEFAULT_SERVER_PORT 55447
#define DEFAULT_SERVER_IP_ADDRESS "127.0.0.1"
#define NUM_OF_RUNS 10

#define ANS_BUF_SIZE sizeof(char) * 7
#define SEND_AGAIN_YES "cont"
#define SEND_AGAIN_NO "exit"
//

int sendall(int s, char *buf, int *len);
char *util_generate_random_data(unsigned int size);
int user_cont();
int send_message(int sockfd, char *buffer, char *ans_buff);
char *int_to_str(unsigned int num);

int main(int argsc, char **argsv)
{
    // code to generate a file bigger then 2.5 mb
    unsigned int data_size = 3 * 1024 * 1024;
    char *rnd_file_buffer = util_generate_random_data(data_size);
    const char  *ip_address;
    unsigned short int port_Address;

    

    if (argsc <= 1)
    {
        // no arguments
    }
    else
    {
        int i = 1;
        while (i < argsc)
        {
            char *arg = argsv[i];
            if (strcmp(arg, "-p") == 0)
            {
                i++;
                if (i == argsc)
                {
                    // error empty arg
                }
                else
                {
                    port_Address = atoi(argsv[i]);
                }
            }
            else if (strcmp(arg, "-ip") == 0)
            {
                i++;
                if (i == argsc)
                {
                    // error empty arg
                }
                else
                {
                    ip_address = argsv[i];
                }
            }
            else if (strcmp(arg, "-algo") == 0)
            {
                i++;
                if (i == argsc)
                {
                    // error empty arg
                }
               
            }
            i++;
        }
    }

    //// finished args reading


    RudpHdr rudp_header;
    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in server;

 
    memset(&server, 0, sizeof(server));

    // Try to create a TCP socket (IPv4, stream-based, default protocol).
    RUDP_Socket* sock=rudp_socket(false,port_Address);
    // If the socket creation failed, print an error message and return 1.
    if (sock == -1)
    {
        perror("socket(2)");
        return 1;
    }

    int opt = 1;

    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set the TCP congestion algorithm option for this socket
    char alg_buf[256];
    socklen_t alg_buf_len;

    strcpy(alg_buf, tcp_algo);
    alg_buf_len = sizeof(alg_buf);

    if (setsockopt(sock, IPPROTO_TCP, TCP_CONGESTION, alg_buf, alg_buf_len) != 0)
    {
        // Option set failed
        perror("setsockopt");
        close(sock);
        return -1;
    }

    // Convert the server's address from text to binary form and store it in the server structure.
    // This should not fail if the address is valid (e.g. "127.0.0.1").
    if (inet_pton(AF_INET, ip_address, &server.sin_addr) <= 0)
    {
        perror("inet_pton(3)");
        close(sock);
        return 1;
    }

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the defined port. Note that the port must be in network byte order,
    // so we first convert it to network byte order using the htons function.
    server.sin_port = htons(port_Address);

    fprintf(stdout, "Sender: Connecting to %s:%d...\n", ip_address, port_Address);

    // Try to connect to the server using the socket and the server structure.
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("connect(2)");
        close(sock);
        return 1;
    }
    fprintf(stdout, "Sender: Successfully connected to the server!\n");

    char *yes = SEND_AGAIN_YES;
    char *no = SEND_AGAIN_NO;
    int ans;
    int bytes_received = 0;
    char ansbuffer[ANS_BUF_SIZE] = {0};
    do
    {

        printf("Sender: waiting for receiver\n");
        recv(sock, ansbuffer, ANS_BUF_SIZE, 0);
        printf("Sender: Sending answer to the server\n");
        int bytes_sent = send(sock, yes, strlen(yes) + 1, 0);
        if (bytes_sent <= 0)
        {
            perror("send(2): ");
            close(sock);
            return 1;
        }

        bytes_received = recv(sock, ansbuffer, ANS_BUF_SIZE, 0);
        if (bytes_received < 0)
        {
            perror("recv(answer)");
            close(sock);
            return 1;
        }
        #ifdef DEBUG
        printf("Sender: recived ack\n");
        printf("Sender: Sending message to the server\n");
        #endif
        // Try to send the message to the server using the socket.
        // bytes_sent = send(sock, rnd_file_buffer, strlen(rnd_file_buffer) + 1, 0);
        bytes_sent = send_message(sock, rnd_file_buffer, ansbuffer);

        // If the message sending failed, print an error message and return 1.
        // If no data was sent, print an error message and return 1. Only occurs if the connection was closed.
        if (bytes_sent <= 0)
        {
            perror("send(message): ");
            close(sock);
            return 1;
        }
        ans = user_cont();

    } while (ans == 1);
    #ifdef DEBUG
    printf("Sender: waiting for receiver\n");
    #endif
    recv(sock, ansbuffer, ANS_BUF_SIZE, 0);
    printf("Sender: Sending no to the server\n");
    int bytes_sent = send(sock, no, strlen(no) + 1, 0);
    if (bytes_sent <= 0)
    {
        perror("send(2): ");
        close(sock);
        return 1;
    }
    
    close(sock);

    fprintf(stdout, "Sender: Connection closed!\n");

    // Return 0 to indicate that the Sender ran successfully.
    return 0;
}

/// @brief asks the user if he wants to continue, y is yes else no
/// @return 1 if user anserwed yes else 0
int user_cont()
{
    char ans = 0;
    char yes = 'y';
    printf("do you want to send file again motherfricker? y/n: \n");
    scanf(" %c", &ans);
    if (ans == yes)
    {
        // printf("Sender: answered yes");
        return 1;
    }
    else
    {
        // printf("Sender: %d \n",ans);
        return 0;
    }
}

int send_message(int sockfd, char *buffer, char *ans_buff)
{
    int size = strlen(buffer) + 1;
    char *str_size = int_to_str(size);
    printf("Sender: size send = %s\n", str_size);
    int unsigned sent;
    send(sockfd, str_size, strlen(str_size) + 1, 0);
    recv(sockfd, ans_buff, ANS_BUF_SIZE, 0);
    sent = sendall(sockfd, buffer, &size);
    recv(sockfd, ans_buff, ANS_BUF_SIZE, 0);
    free(str_size);
    return sent == 0;
}
char *int_to_str(unsigned int num)
{
    int length = snprintf(NULL, 0, "%d", num);
    char *str = malloc(length + 1);
    snprintf(str, length + 1, "%d", num);
    return str;
}
int sendall(int s, char *buf, int *len)
{
    int total = 0;        // how many bytes we've sent
    int bytesleft = *len; // how many we have left to send
    int n;

    while (total < *len)
    {
        n = send(s, buf + total, bytesleft, 0);
        if (n == -1)
        {
            break;
        }
        total += n;
        bytesleft -= n;
    }

    *len = total; // return number actually sent here

    return n == -1 ? -1 : 0; // return -1 on failure, 0 on success
}


char *util_generate_random_data(unsigned int size)
{
    char *buffer = NULL;
    // Argument check.
    if (size == 0)
        return NULL;
    buffer = (char *)calloc(size, sizeof(char));
    // Error checking.
    if (buffer == NULL)
        return NULL;
    // Randomize the seed of the random number generator.
    // srand(time(NULL));
    for (unsigned int i = 0; i < size - 1; i++)
        *(buffer + i) = 'a';
    *(buffer + size - 1) = '\n';
    return buffer;
}
