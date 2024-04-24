#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include "RUDP_API.h" 

#define DEFAULT_SERVER_PORT 55447
#define BUFFER_SIZE 5 * 1024 * 1024
#define SEND_AGAIN_YES "cont"

int main(int argc, char **argv) {
    int port = DEFAULT_SERVER_PORT;
    

    // Parsing command-line arguments
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
                port = atoi(argv[i + 1]);
            } 
        }
    }

    // Socket setup
    int sockfd = 0;
    RudpPacket *sock = rudp_socket(true,port); // Create RUDP socket
    if (sockfd < 0) {
        perror("rudp_socket");
        return 1;
    }

    // // Bind to port
    // if (rudp_bind(sockfd, port) < 0) {
    //     perror("rudp_bind");
    //     rudp_close(sockfd);
    //     return 1;
    // }

    fprintf(stdout, "Listening for incoming connections on port %d...\n", port);

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (1) {
        int iterations = 0;
        int bytes_received =0;
        double roundtime=0.0;
        int client_sock = rudp_accept(sock);
        static unsigned long amountBytesReceived = 0;
        static double amountOfTime =0;
        if (client_sock == 0) {
            perror("rudp_accept");
            rudp_close(sockfd);
            return 1;
        }

        // fprintf(stdout, "Sender %s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        char buffer[BUFFER_SIZE];
        char ansbuffer[sizeof(char) * 7] = {0};

        do {
            send(client_sock, "rdy", 4, 0); // Send ready signal

            // Receive request from sender
            recv(client_sock, ansbuffer, sizeof(ansbuffer), 0);

            if (strcmp(ansbuffer, SEND_AGAIN_YES) == 0) {
                iterations =iterations+1;
                // Receive message using RUDP
                bytes_received = rudp_recv(client_sock, buffer, BUFFER_SIZE);
                if (bytes_received < 0) {
                    perror("rudp_recv");
                    rudp_close(client_sock);
                    rudp_close(sockfd);
                    return 1;
                } else if (bytes_received == 0) {
                    // Client disconnected
                    fprintf(stdout, "Receiver: Sender %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    rudp_close(client_sock);
                    continue;
                }

                // Process received message

                // Acknowledge message
                send(client_sock, "ack", strlen("ack"), 0);

                struct timeval t_start, t_end;
                gettimeofday(&t_end,NULL);
                double roundtime = (t_end.tv_sec-t_start.tv_sec)*1000 +((float)t_end.tv_usec-t_start.tv_usec) /1000;
                amountBytesReceived = amountBytesReceived + bytes_received;
                amountOfTime = amountOfTime + roundtime;
                
                
                
            }
        }
        while (strcmp(ansbuffer, SEND_AGAIN_YES) == 0);
        printf("\nStatistics : \n");
        fprintf(stdout, "Total time = %.2f ms\n ",amountOfTime);
        printf("Reciever: recieved %d bytes \n", bytes_received);
        printf("num of rounds = %d\n", iterations);
        fprintf(stdout, "Avg RTT = %.2f\n", roundtime/iterations);
        double avgthroughput = 0;
        if(roundtime != 0){
            avgthroughput = (double)amountBytesReceived *8/ ((1.0/1000)*roundtime)/1000000;    
        }
        fprintf(stdout,"Avg throughput = %f Mbps \n",avgthroughput);       
        rudp_close(client_sock);
        fprintf(stdout, "Receiver: Sender %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));


    }

    rudp_close(sockfd);
    fprintf(stdout, "Receiver: Receiver finished!\n");

    return 0;
}
