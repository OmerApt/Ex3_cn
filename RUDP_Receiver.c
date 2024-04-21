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
        int client_sock = rudp_accept(sock);
        if (client_sock == 0) {
            perror("rudp_accept");
            rudp_close(sockfd);
            return 1;
        }

        // fprintf(stdout, "Sender %s:%d connected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        char buffer[BUFFER_SIZE];
        do {
            send(client_sock, "rdy", 4, 0); // Send ready signal

            // Receive request from sender
            char ansbuffer[sizeof(char) * 7] = {0};
            recv(client_sock, ansbuffer, sizeof(ansbuffer), 0);

            if (strcmp(ansbuffer, SEND_AGAIN_YES) == 0) {
                // Receive message using RUDP
                int bytes_received = rudp_recv(client_sock, buffer, BUFFER_SIZE, 0);
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
                // Note: In RUDP, you might need to handle data integrity and sequence numbers here

                // Acknowledge message
                send(client_sock, "ack", strlen("ack"), 0);
            }
        } while (strcmp(ansbuffer, SEND_AGAIN_YES) == 0);

        fprintf(stdout, "Receiver: Sender %s:%d disconnected\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        rudp_close(client_sock);
    }

    rudp_close(sockfd);
    fprintf(stdout, "Receiver: Receiver finished!\n");

    return 0;
}
