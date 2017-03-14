#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>     
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#include "packet.h"

#define BUFSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(0);
}

// Function that sends an acknowledgment to the server
void send_ack(int seq_num, int sockfd, struct sockaddr_in serv_addr)
{
    struct Packet ack;
    ack.sequence = seq_num;
    ack.type = TYPE_ACK;
    ack.length = 0;
    strcpy(ack.data, "");

    if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      error("ERROR sending acknowledgment");

    printf("Sent ack %d.\n", seq_num);
}

int main(int argc, char *argv[])
{
    // Example UDP client:
        // https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c

    // Initialize variables
    int sockfd, portno, n, result;
    char* filename;

    FILE* f;
    int num_bytes;
    int f_index = 0;

    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char* hostname;
    char buf[BUFSIZE];

    struct Packet request;
    struct Packet response;
    struct Packet* buffer;
    int buffer_index;

    int expected_sequence = 0;

    // Parse command line arguments
        // hostname
        // port number
        // filename
        // etc.

    if (argc != 4) {
     fprintf(stderr, "usage: %s <hostname> <port> <filename>\n", argv[0]);
     exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];


    // Set up UDP socket
        // use socket() call
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
      error("ERROR opening socket");

    server = gethostbyname(hostname);
    if (server == NULL) {
      fprintf(stderr,"ERROR, no such host as %s\n", hostname);
      exit(0);
    }
    
    // Update address information
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
      (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    // Create the request
    memset((char *) &request, 0, sizeof(request));
    strcpy(request.data, filename);
    request.length = strlen(filename) + 1;
    request.type = TYPE_REQUEST;

    // Send the request
    serverlen = sizeof(serveraddr);
    if (sendto(sockfd, &request, sizeof(struct Packet), 0, (struct sockaddr *) &serveraddr, serverlen) < 0)
      error("ERROR sending request");

    // DEBUGGING
    printf("Sent request for file %s\n", filename);
    printf("The size of the packet is: %lu\n", sizeof(struct Packet));

    print_packet(request);

    // TODO: Start timer for timeout

    // TODO: Create a copy of the file for the client 
    // in order to detect corruption


    // TODO: Wait for response from server
    while (1) {

        // // TODO: If there is a timeout, resend the request

        // // Look for next sequence number in buffer
        // // Continue this until it is not found, or until we have already found
        // // the last sequence number
        // while (not_found == false && end == false) {
        //   for (int ix = 0; ix < buffer_index; ix++) {
        //     if (expected_sequence == buffer[ix].sequence) {
        //       int num_bytes = fwrite(buffer[ix].data, 1, buffer[ix].length, f);
        //       f_index += num_bytes;
        //       expected_sequence++;
        //       break;
        //     }
        //   }
        //   not_found == true;
        // } 

        // // Otherwise, wait for/handle response packet
        // if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) < 0) {
        //   printf("Packet was not received\n");
        // }

        // // Packet received out of order, place in buffer.
        // if (response.sequence != expected_sequence) {
        //   printf("Packet was received out of order: expected %d, received %d.\n", expected_sequence, response.sequence);
        //   send_ack(response.sequence, sockfd, serveraddr);

        //   // Place out of order packet in buffer
        //   buffer[buffer_index] = response;
        //   buffer_index++;

        // }
        // // Packet received in order, write it to the file.
        // else {
        //   send_ack(expected_sequence, sockfd, serveraddr);
        //   expected_sequence++;

        //   int num_bytes = fwrite(response.data, 1, response.length, f);
        //   // TODO: check if correct number of bytes were written to file
        //   f_index += num_bytes;
        // }

        if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) < 0) {
           printf("Packet was not received\n");
           break;
        }
        
        // Packet received in order
        if (response.sequence == expected_sequence) {
          send_ack(response.sequence, sockfd, serveraddr);
          num_bytes = fwrite(response.data, 1, response.length, f);
          f_index += num_bytes;
          expected_sequence++;
          if (response.type == TYPE_END_DATA) break;
        }
        // TODO: Packet received out of order
        else {

        }
    }

    // REMOVE: Print server's reply
    //n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    //if (n < 0) 
    //  error("ERROR in recvfrom");
    //printf("Echo from server: %s", buf);
    //return 0; 

    // Close the socket
    close(sockfd);

    // TODO: Close the client's copy of the file
    //fclose(f);
    //free(filecopy);
    return 0;
}