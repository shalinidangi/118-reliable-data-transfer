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
#include <unistd.h>

#include "packet.h"

#define BUFSIZE 1024

void error(char *msg) {
  perror(msg);
  exit(0);
}

// Find milliseconds difference between calls to clock()
double diff_in_ms(clock_t c1, clock_t c2) {
  return (c2 - c1) / (CLOCKS_PER_SEC/1000000);
}

// Sends initial SYN packet to server to establish a connection
void send_syn(int sockfd, struct sockaddr_in serv_addr) {
  struct Packet syn;
  syn.sequence = 0;
  syn.ack = 0;
  syn.type = TYPE_SYN;
  syn.length = 0;
  strcpy(syn.data, "");

  if (sendto(sockfd, &syn, sizeof(syn), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR sending SYN");
}

// Sends an acknowledgment to the server
// Indicates the client has received packet beginning with sequence number seq_num
void send_ack(int seq_num, int ack_num, int sockfd, struct sockaddr_in serv_addr) {
  struct Packet ack;
  // [TODO]: distinguish between seq_num and ack_num
  ack.sequence = seq_num;
  ack.ack = ack_num;
  ack.type = TYPE_ACK;
  ack.length = 0;
  strcpy(ack.data, "");

  if (sendto(sockfd, &ack, sizeof(ack), 0, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    error("ERROR sending acknowledgment");

  printf("Sending packet %d\n", seq_num);
}

int main(int argc, char *argv[]) {
  // Example UDP client: 
  // https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c

  // Variable declarations
  int sockfd, portno, ix;
  char* filename;

  FILE* f;

  socklen_t serverlen;
  struct sockaddr_in serveraddr;
  struct hostent *server;
  char* hostname;

  int connection_established = false;

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
  bool should_buffer = true;



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

  // [TODO] Fix timeout
  // // Set timeout value of sockfd to 500 ms
  // struct timeval tv;
  // tv.tv_sec = 0;
  // tv.tv_usec = 500000;
  // if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
  //   perror("Error");
  // }

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
  clock_t timer_start = clock();

  bool retransmission = false;

  // Wait for SYN-ACK packet
  while (1) {
    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) >= 0) {
      if (response.type == TYPE_SYN_ACK) {
        connection_established = true;
        break;
      }
      else {
        error("ERROR recvfrom() failed");
      }
    }
  }

  // If SYN-ACK received successfully, send ACK and request packet
  if (connection_established) {
    // Special ACK with sequence number = acknowledgment number = 1 to establish connection
    send_ack(1, 1, sockfd, serveraddr);

    // Create the request
    memset((char *) &request, 0, sizeof(request));
    strcpy(request.data, filename);
    request.length = strlen(filename) + 1;
    request.type = TYPE_REQUEST;

    // Open file for writing received packets
    // [TODO]: rename output file for multiple requests on same connection
    // [TODO]: write settings "wb" vs "ab"
    char* output = "received.data";

    f = fopen(output, "ab");
    if (f == NULL)
      error("ERROR Failed to open file");

    // Send the request
    serverlen = sizeof(serveraddr);
    if (sendto(sockfd, &request, sizeof(struct Packet), 0, (struct sockaddr *) &serveraddr, serverlen) < 0)
      error("ERROR sending request");

    // DEBUG
    printf("Sent request for file %s\n", filename);

    // [TODO]: Timer for request packet acknowledgment?

    // Wait for responses from server
    while (1) {

      // If last packet has already been written,
      // no more responses are expected from the server.
      if (last_packet_written) 
        break;

      // Handle response packet from server
      if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr *) &serveraddr, &serverlen) < 0) {
         error("ERROR Packet was not received\n");
      }
      printf("Receiving packet %d\n", response.sequence);

      // Packet received in order
      if (response.sequence == expected_sequence) {

        // Acknowledge reception of this packet
        // [TODO]: Sequence number == Ack number (verify this)
        send_ack(response.sequence, response.sequence, sockfd, serveraddr);
        
        // Write the packet to the file
        if (fwrite(response.data, 1, response.length, f) != response.length)
          error("ERROR write failed");
        
        // Update expected sequence and window
        expected_sequence += PACKET_SIZE;
        end += PACKET_SIZE;
        // DEBUG
        printf("Current window start: %i, Current window end: %i\n", expected_sequence, end); 

        // Wrote last packet of this file
        if (response.type == TYPE_END_DATA) {
          last_packet_written = true;
          break;
        }

        // Check for next packets in buffer
        ix = 0;
        while (ix < 5) {
          // If the next expected sequence number is in the buffer AND it is valid
          // write it to the file.  Then invalidate it in the buffer. 
          if (buffer[ix].sequence == expected_sequence && valid[ix] == true) {
            
            // Write the packet to the file
            if (fwrite(buffer[ix].data, 1, buffer[ix].length, f) != buffer[ix].length)
              error("ERROR write failed");
            
            // Update expected_sequence and window
            expected_sequence += PACKET_SIZE;
            end += PACKET_SIZE;
            // DEBUG
            printf("Current window start: %i, Current window end: %i\n", expected_sequence, end);

            // Wrote last packet of this file
            if (buffer[ix].type == TYPE_END_DATA) {
              last_packet_written = true;
              break;
            }

            // Invalidate this slot so it can be used again
            valid[ix] = false;
            // Start looking for the next sequence number at the beginning of the buffer
            ix = 0;
          }

          // Next sequence number was not found at this index. Keep looking in buffer.
          else {
            ix++;
          }
        }
      } // END IN-ORDER HANDLING

      // Packet received out of order
      else {

        // Packet received is in pre-window range. Discard it.
        // Handles duplicate reception of a packet.
        if (response.sequence < expected_sequence) {
          printf("ERROR packet received is in pre-window range. Sequence: %i\n", response.sequence);
          // DEBUG
          printf("Current window start: %i, Current window end: %i\n", expected_sequence, end);
          send_ack(response.sequence, response.sequence, sockfd, serveraddr);

        }

        // Packet received is in post-window range. Discard it.
        // This shoudn't happen.
        else if (response.sequence > end) {
          printf("ERROR packet received is in post-window range. Sequence: %i\n", response.sequence);
          // DEBUG
          printf("Current window start: %i, Current window end: %i\n", expected_sequence, end);
          send_ack(response.sequence, response.sequence, sockfd, serveraddr);
        }

        // Packet received is in acceptable range.
        else {
          // Check if it is already in the buffer.
          // DEBUG
          printf("Packet: %i already exists in buffer. Ignore it.\n", response.sequence);
          ix = 0;
          should_buffer = true;
          while (ix < 5) {
            if (buffer[ix].sequence == response.sequence && valid[ix] == true) {
              should_buffer = false;
              break;
            }
            else {
              ix++;
            }
          }

          // DEBUG
          printf("Place packet: %i in buffer\n", response.sequence);
          // If packet does not already exist in buffer. Buffer it.
          if (should_buffer) {
            // Find the next open slot in the buffer
            bool found_slot = false;
            int ix = 0;
            while (!found_slot && ix < 5) {
              if (valid[ix] == false) {
                found_slot = true;
                break;
              }
              ix++;
            }

            if (ix > 4) {
              error("ERROR too many OoO packets in buffer");
            }

            // Buffer this out-of-order packet at this slot
            buffer[ix] = response;
            valid[ix] = true;
          }

          // DEBUG
          printf("Acking OoO packet %i\n", response.sequence);
          // Acknowledge reception of this packet
          send_ack(response.sequence, response.sequence, sockfd, serveraddr);
        }
      } // END OUT-OF-ORDER HANDLING

    } // END SERVER RESPONSE WHILE
  } 
  
  // Close the socket
  close(sockfd);
  return 0;
}