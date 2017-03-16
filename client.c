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
    int sockfd, portno, n, result, ix;
    char* filename;

    FILE* f;

    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char* hostname;
    char buf[BUFSIZE];

    struct Packet request;
    struct Packet response;

    struct Packet buffer[5];
    // restrict buffer size to current window
    int expected_sequence = 1;
    int end = WINDOW_SIZE + expected_sequence;
    // DEBUG
    printf("Current window start: %i, Current window end: %i\n", expected_sequence, end);
    
    bool valid[5] = {false};
    bool last_packet_written = false;

    int connection_established = false;

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

      // TODO: rename output file for multiple requests on same connection
      // TODO: write settings "wb" vs "ab"
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

      // Wait for response from server
      while (1) {

          // TODO: If there is a timeout, resend the request

          // If last packet has already been written,
          // no more responses are expected from the server
          if (last_packet_written) break;

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
            
            // Write the packet to the file
            if (fwrite(response.data, 1, response.length, f) != response.length)
              error("ERROR write failed");
            
            // Update expected sequence and window
            expected_sequence += PACKET_SIZE;
            end += PACKET_SIZE;
            // DEBUG
            printf("Current window start: %i, Current window end: %i\n", expected_sequence, end); 

            if (response.type == TYPE_END_DATA) {
              // This was the last packet for this file
              last_packet_written = true;
            }

            int ix = 0;
            // Check for next packets in buffer
            while (ix < 5) {
              // If the next expected sequence number is in the buffer AND it is valid
              // write it to the file.  Then invalidate it in the buffer. 
              if (buffer[ix].sequence == expected_sequence && valid[ix] == true) {
                send_ack(buffer[ix].sequence, buffer[ix].sequence, sockfd, serveraddr);
                printf("Sending packet %d\n", buffer[ix].sequence);
                
                // Write the packet to the file
                if (fwrite(buffer[ix].data, 1, buffer[ix].length, f) != buffer[ix].length)
                  error("ERROR write failed");
                
                // Update expected_sequence and window
                expected_sequence += PACKET_SIZE;
                end += PACKET_SIZE;
                // DEBUG
                printf("Current window start: %i, Current window end: %i\n", expected_sequence, end);

                if (buffer[ix].type == TYPE_END_DATA) {
                  // Wrote the last packet for this file. 
                  last_packet_written = true;
                  break;
                }

                // Invalidate this slot so it can be used again.
                valid[ix] = false;
                // Start looking for the next sequence number at the beginning of the buffer.
                ix = 0;
              }

              // Next sequence number not found at this index. Keep looking in buffer.
              else {
                ix++;
              }
            }
          }

          // Packet received out of order
          else {

            // Packet received is in pre-window range. Discard it.
            if (response.sequence < expected_sequence) {
              printf("ERROR packet received is in pre-window range");
            }

            // Packet received is in post-window range. Discard it.
            else if (response.sequence > end) {
              printf("ERROR packet receives is in post-window range");
            }

            // Packet received is in acceptable range. Buffer it.
            else {
              // Find the next open slot in the buffer
              bool found_slot = false;
              int ix = 0;
              while (!found_slot && ix < 5) {
                if (valid[ix] == false) {
                  found_slot = true;
                }
                ix++;
              }

              if (ix > 4) {
                error("ERROR too many OoO packets in buffer");
              }
              // Buffer this out-of-order packet at this slot
              buffer[ix] = response;
              valid[ix] = 1;
            }
          }
        }
      }
    
    // Close the socket
    close(sockfd);
    return 0;
}