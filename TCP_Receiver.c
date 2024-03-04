// includes
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
#include <sys/time.h>
//

// defines
#define DEFAULT_SERVER_PORT 55447
#define NUM_OF_CLINETS 1
#define DEFAULT_ALGO "reno"
#define BUFFER_SIZE 5 * 1024 * 1024
#define SEND_AGAIN_YES "cont"
#define SEND_AGAIN_NO "exit"
#define FILE_SIZE_DESC 30
//
int recive_message(int sockfd, char *buffer);

int main(int argsc, char **argsv)
{

    // char *rnd_file_buffer = util_generate_random_data(2500000);
    char *tcp_algo = DEFAULT_ALGO;
    // char *ip_address = DEFAULT_SERVER_IP_ADDRESS;
    int port_Address = DEFAULT_SERVER_PORT;

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
            // printf("args num is %d\n", argsc);
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
                    // printf("%d\n", port_Address);
                }
            }
            else if (strcmp(arg, "-algo") == 0)
            {
                i++;
                if (i == argsc)
                {
                    // error empty arg
                }
                else
                {
                    // check if value is reno or cubic
                    tcp_algo = argsv[i];
                }
            }
            i++;
        }
    }

    //// finished args reading

    // The variable to store the socket file descriptor.
    int sock = -1;

    // The variable to store the server's address.
    struct sockaddr_in server;

    // The variable to store the client's address.
    struct sockaddr_in client;

    // Stores the client's structure length.
    socklen_t client_len = sizeof(client);

    // The variable to store the socket option for reusing the server's address.
    int opt = 1;

    // Reset the server and client structures to zeros.
    memset(&server, 0, sizeof(server));
    memset(&client, 0, sizeof(client));

    // Try to create a TCP socket (IPv4, stream-based, default protocol).
    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1)
    {
        perror("socket(2)");
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
        perror("setsockopt(congestion)");
        close(sock);
        return 1;
    }

    // Set the socket option to reuse the server's address.
    // This is useful to avoid the "Address already in use" error message when restarting the server.
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt(2)");
        close(sock);
        return 1;
    }

    // Set the server's address to "0.0.0.0" (all IP addresses on the local machine).
    server.sin_addr.s_addr = INADDR_ANY;

    // Set the server's address family to AF_INET (IPv4).
    server.sin_family = AF_INET;

    // Set the server's port to the specified port. Note that the port must be in network byte order.
    server.sin_port = htons(port_Address);

    // Try to bind the socket to the server's address and port.
    if (bind(sock, (struct sockaddr *)&server, sizeof(server)) < 0)
    {
        perror("bind(2)");
        close(sock);
        return 1;
    }

    // Try to listen for incoming connections.
    if (listen(sock, NUM_OF_CLINETS) < 0)
    {
        perror("listen(2)");
        close(sock);
        return 1;
    }

    fprintf(stdout, "Listening for incoming connections on port %d...\n", port_Address);

    // double total_of_all;

    // The server's main loop.
    // int counter =0;
    while (1)
    {
        // Try to accept a new client connection.
        int client_sock = accept(sock, (struct sockaddr *)&client, &client_len);
        struct timeval t_start, t_end;
        double total = 0;
        double roundtime = 0;
        int iterations = 0;
        unsigned long totalBytesreceived = 0;
        // timer_t end_time = 0;
        // If the accept call failed, print an error message and return 1.
        if (client_sock < 0)
        {
            perror("accept(2)");
            close(sock);
            return 1;
        }

        // Print a message to the standard output to indicate that a new client has connected.
        fprintf(stdout, "Sender %s:%d connected\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        // Create a buffer to store the received answer if continue or exit
        char ansbuffer[sizeof(char) * 7] = {0};
        // Create a buffer to store the received message.
        char buffer[BUFFER_SIZE] = {0};

        do
        {
            send(client_sock, "rdy", 4, 0);
            int ans_bytes_received = 0;
            // Receive a message from the client and store it in the buffer.will be yes first
            ans_bytes_received = recv(client_sock, ansbuffer, BUFFER_SIZE, 0);
            #ifdef DEBUG
            printf("Reciever: receiving answer\n");
            #endif

            if (ans_bytes_received < 0)
            {
                perror("recv(answer)");
                close(client_sock);
                close(sock);
                return 1;
            }

            int bytes_sent = send(client_sock, "ack", strlen("ack"), 0);
            #ifdef DEBUG
            printf("Reciever: sent ack\n");
            #endif

            if (bytes_sent <= 0)
            {
                perror("send(message): ");
                close(client_sock);
                close(sock);
                return 1;
            }
            if (strcmp(ansbuffer, SEND_AGAIN_YES) == 0)
            {
                iterations++;

                gettimeofday(&t_start, NULL);
                #ifdef DEBUG
                printf("Reciever: round %d\n", iterations);
                #endif
                // bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
                int bytes_received = recive_message(client_sock, buffer);
                #ifdef DEBUG
                printf("Reciever: receiving message\n");
                #endif
                // If the message receiving failed, print an error message and return 1.
                if (bytes_received < 0)
                {
                    perror("recv(message)");
                    close(client_sock);
                    close(sock);
                    return 1;
                }
                // If the amount of received bytes is 0, the client has disconnected.
                // Close the client's socket and continue to the next iteration.
                else if (bytes_received == 0)
                {
                    fprintf(stdout, "Reciever: Sender %s:%d disconnected\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));
                    close(client_sock);
                    continue;
                }
                gettimeofday(&t_end, NULL);
                totalBytesreceived = totalBytesreceived + bytes_received;
                roundtime = (t_end.tv_sec - t_start.tv_sec) * 1000 + ((float)t_end.tv_usec - t_start.tv_usec) / 1000;
                total = total + roundtime;

                fprintf(stdout, "Reciever: time of round = %.2f ms\n", roundtime);
                printf("Reciever: recieved %d bytes\n", bytes_received);
            }
        } while (strcmp(ansbuffer, SEND_AGAIN_YES) == 0);
        printf("\nStatistics:\n");
        fprintf(stdout, "Total time = %.2f ms\n", total);
        printf("num of rounds = %d\n", iterations);
        fprintf(stdout, "Avg rtt = %.2f\n", total / iterations);
        double avgthroughput = 0;
        if (total != 0)
        {
            avgthroughput = (double)totalBytesreceived * 8;
            avgthroughput = avgthroughput / ((1.0 / 1000) * total);
            avgthroughput = avgthroughput / 1000000;
        }
        fprintf(stdout, "Avg throughput = %fmbps\n", avgthroughput);
        printf("Algorithm used = %s\n", tcp_algo);

        // Close the client's socket and continue to the next iteration.
        close(client_sock);

        fprintf(stdout, "Reciever: Sender %s:%d disconnected\n", inet_ntoa(client.sin_addr), ntohs(client.sin_port));

        fprintf(stdout, "Reciever: Reciever finished!\n");

        return 0;
    }
}

int recive_message(int sockfd, char *buffer)
{
    char *str_size = malloc(FILE_SIZE_DESC);
    int bytes_recieved = recv(sockfd, str_size, FILE_SIZE_DESC, 0);
    if (bytes_recieved < 0)
    {
        free(str_size);
        perror("err_Reciver: ");
        send(sockfd, "err", 4, 0);
        return -1;
    }
    else
    {
        send(sockfd, "ack", 4, 0);
        #ifdef DEBUG
        printf("Reciver: sent ack size recived\n");
        #endif
    }
    int size_left = atoi(str_size);
    bytes_recieved = 0;
    unsigned int total_bytes_recived = 0;
    do
    {
        bytes_recieved = recv(sockfd, buffer, strlen(buffer) + 1, 0);
        if (bytes_recieved < 0)
        {
            free(str_size);
            perror("err_Reciver: ");
            return -1;
        }
        size_left = size_left - bytes_recieved;
        total_bytes_recived = total_bytes_recived + bytes_recieved;

    } while (size_left > 0);
    send(sockfd, "ack", 4, 0);
    #ifdef DEBUG
    printf("Reciver: sent ack msg recived\n");
    #endif
    free(str_size);

    return total_bytes_recived;
}
