#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>     
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include <stdbool.h>

#include "packet.h"

#define BUFSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(0);
}

void send_syn(int sockfd, struct sockaddr_in serv_addr) 
{
  struct Packet syn;
  syn.sequence = 0;
  syn.ack = 0;
  syn.type = TYPE_SYN;
  syn.length = 0;
  strcpy(syn.data, "");

  if (sendto(sockfd, &syn, sizeof(syn), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR sending SYN");

  // printf("Sent SYN\n");
}

// Function that sends an acknowledgment to the server
// seq_num indicates next expected sequence number from the server
// ack_num acknowledges 
void send_ack(int seq_num, int ack_num, int sockfd, struct sockaddr_in serv_addr)
{
    struct Packet ack;
    // TODO: distinguish between seq_num and ack_num
    ack.sequence = seq_num;
    ack.ack = ack_num;
    ack.type = TYPE_ACK;
    ack.length = 0;
    strcpy(ack.data, "");

    if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
      error("ERROR sending acknowledgment");

    // printf("Sent ack %d.\n", seq_num);
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

    int connection_established = false;

    int expected_sequence = 1;

    // Parse command line arguments
    if (argc != 4) {
     fprintf(stderr, "usage: %s <hostname> <port> <filename>\n", argv[0]);
     exit(1);
    }

    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];


    // Set up UDP socket
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

    // Establish connection with server
    // Send SYN packet
    send_syn(sockfd, serveraddr);
    printf("Sending packet SYN\n");

    // Wait for SYN-ACK packet
    while (1) {
      if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) < 0) {
        error("ERROR Packet was not received\n");
      }

      if (response.type == TYPE_SYN_ACK) {
        connection_established = true;
        break;
      }
    }

    // If successfully received SYN-ACK, send ACK and request file
    if (connection_established) {
      // Special ACK with sequence number = acknowledgment number = 1 to establish connection
      send_ack(1, 1, sockfd, serveraddr);

      // Create the request
      memset((char *) &request, 0, sizeof(request));
      strcpy(request.data, filename);
      request.length = strlen(filename) + 1;
      request.type = TYPE_REQUEST;

      char* output = "received.data";

      f = fopen(output, "ab");
      if (f == NULL)
        error("ERROR Failed to open file");

      // Send the request
      serverlen = sizeof(serveraddr);
      if (sendto(sockfd, &request, sizeof(struct Packet), 0, (struct sockaddr *) &serveraddr, serverlen) < 0)
        error("ERROR sending request");

      // DEBUGGING
      printf("Sent request for file %s\n", filename);
      printf("The size of the packet is: %lu\n", sizeof(struct Packet));

      print_packet(request);

      // TODO: Start timer for timeout

      // TODO: Wait for response from server
      while (1) {

          // TODO: If there is a timeout, resend the request


          // Handle response packet from server
          if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) < 0) {
             error("ERROR Packet was not received\n");
          }
          
          printf("Receiving packet %d\n", response.sequence);

          // Packet received in order
          if (response.sequence == expected_sequence) {

            // Next sequence number should be the ack number of the packet received
            // Next ack number should be the initial sequence number + the number of bytes received
            send_ack(response.sequence, response.sequence, sockfd, serveraddr);
            printf("Sending packet %d\n", response.sequence);
            
            num_bytes = fwrite(response.data, 1, response.length, f);
            if (num_bytes < 0)
              printf("Write failed");
            f_index += num_bytes;
            expected_sequence += PACKET_SIZE;

            if (response.type == TYPE_END_DATA) 
              break;
          }
          // TODO: Packet received out of order
          else {

          }
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