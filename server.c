#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>

#include "packet.h"
#include "vector.h"

#define MAX_BUFFER_SIZE 4096


/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/**
 * Calculate the number of packets for the given file 
 */

int number_of_packets(FILE * f) {
  // Determine size of file and number of packets needed 
  int num; 
  fseek(f, 0, SEEK_END); 
  int file_size = ftell(f); 
  rewind(f);
  printf("DEBUG: The requested file is %d bytes.\n", file_size); 
  num = file_size / PACKET_DATA_SIZE;
  if (file_size % PACKET_DATA_SIZE) {
    num++; 
  }
  return num;  
}

/**
 * Handle ACKS from clients and returns the ACK number
 */

int handle_ack(int sock) {
  int len;
  struct Packet rec_pkt;  

  len = recvfrom(sock, &rec_pkt, sizeof(struct Packet), 0, NULL, NULL);
  if (len < 0) {
    error("Error receiving packet from client!\n");
  }

  if (rec_pkt.type == TYPE_ACK) {
    printf("Received ACK %d from client\n", rec_pkt.ack);
    return rec_pkt.ack; 
  }
  else {
    printf("Received packet of type %d instead of TYPE_ACK\n", rec_pkt.type);
    return -1; 
  }
}

/**
 * Divides the requested file into data packets
 */
struct Packet* packetize_file(FILE * f) {
  struct Packet* packets = NULL;
  struct Packet data_packet; 
  int file_size;
  int num_packets = 0;   

  num_packets = number_of_packets(f); 
  printf("DEBUG: The number of packets needed for the file is %d\n", num_packets); 
  
  // Create the packets array
  packets = (struct Packet*) malloc(sizeof(struct Packet) * num_packets);
  
  if (packets == NULL) {
    error("Creating packets array failed.\n");
  }

  // Divide file into packets
  int current_seq_num = 1; 
  int i; 
  for (i = 0; i < num_packets; i++) {
    memset((char *) &data_packet, 0, sizeof(data_packet));
    data_packet.sequence = current_seq_num;
    current_seq_num += PACKET_SIZE; 
    data_packet.length = fread(data_packet.data, sizeof(char), PACKET_DATA_SIZE, f); 

    // Set last data packet
    if (i == num_packets - 1) {
      data_packet.type = TYPE_END_DATA; 
    }
    else {
      data_packet.type = TYPE_DATA; 
    }

    packets[i] = data_packet;
  }

  printf("DEBUG: Content of packet array: \n");
  print_packet_array(packets, num_packets); 
  return packets;
}

int main(int argc, char *argv[]) {
  
  /**
   * Code based on examples from: 
   * https://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
   * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#intro
   * https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c
   * CS118 Discussion Slides
   */
  
  int sock_fd, port, recv_len, yes = 1; 
  unsigned int cli_len;
  struct sockaddr_in serv_addr, client_addr;
  struct hostent *hostp; // Client host info
  char *hostaddrp; // Host address string
  
  bool established_connection = false; // used for handshake 

  struct Packet received_packet; // Packet received from client
  FILE * f; 
  struct Packet* packets; //Array of packets for file 
  int n_packets; 

  int i; // Use for loops
  
  
  // Parse port number 
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  port = atoi(argv[1]);

  // Open socket connection with UDP
  if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("Error opening socket");
    exit(1);
  }

  // Set options for socket
  if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
    perror("Error setting socket options");
    exit(1);
  }

  // Bind socket to host address and socket
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  serv_addr.sin_port = htons(port);

  if (bind(sock_fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1) {
    perror("Error binding");
    exit(1);
  }

  cli_len = sizeof(client_addr);

  printf("Waiting for incoming connections...\n"); 
  while(1) {
    
    // Receive a packet from client  
    recv_len = recvfrom(sock_fd, &received_packet, sizeof(struct Packet), 0,
                       (struct sockaddr *) &client_addr, &cli_len);
    
    if (recv_len < 0) {
      error("No data received!\n");
    }

    printf("DEBUG: Receving a request! The contents of the packet are: \n");
    print_packet(received_packet);  
    printf("DEBUG: The size of the packet is: %d\n", recv_len);

    printf("Established connection is %s\n", established_connection ? "true" : "false");    

    // HANDSHAKE: Send out SYN-ACK to client
    if (received_packet.type == TYPE_SYN && established_connection == false) {
      struct Packet syn_ack_packet;
      syn_ack_packet.sequence = 0; 
      syn_ack_packet.type = TYPE_SYN_ACK;
      syn_ack_packet.ack = received_packet.ack + 1; 
      if (sendto(sock_fd, &syn_ack_packet, sizeof(struct Packet), 0, 
                (struct sockaddr *) &client_addr, cli_len) > 0 ) {
            printf("Sending packet %d %d SYN\n", syn_ack_packet.sequence, WINDOW_SIZE);
      }
      else {
        printf("Error writing SYN-ACK packet\n"); 
      }
    }

    // HANDSHAKE: Establish connection
    if (received_packet.type == TYPE_ACK && established_connection == false) {
      established_connection = true; 
      printf("Receiving %d\n", received_packet.ack);      
    }

    if (received_packet.type == TYPE_REQUEST && established_connection == true) {
      printf("DEBUG: File to be opened: %s\n", received_packet.data);
      f = fopen(received_packet.data, "r");
      
      if (f == NULL) {
        error("Error opening file.\n");
      }
      n_packets = number_of_packets(f); 
      packets = packetize_file(f);

      if (packets == NULL) {
        error("File failed to be packetized\n");
      }
      
      // Send out packets by window
      // [TODO]: Move this to a function 
      int next_packet_num = 0; 
      int base = 0; 
      bool all_sent = false; 
      int window_num = WINDOW_SIZE / PACKET_SIZE; 

      // Initialize unacked_packets vector
      VECTOR_INIT(unacked_packets); 
      for (i = 0; i < base + window_num; i++) {
        VECTOR_ADD(unacked_packets, &packets[i].sequence); 
        printf("DEBUG: Initializing the vector array with sequence value - %d\n", *(VECTOR_GET(unacked_packets, int*, i)));  
      } 


      while (all_sent == false || base < next_packet_num) {

        // Send the current packets in the window
        while (all_sent == false && next_packet_num < base + window_num) {
          
          // Send packet
          if (sendto(sock_fd, &packets[next_packet_num], sizeof(struct Packet), 0, 
             (struct sockaddr *) &client_addr, cli_len) > 0 ) {
            printf("Sending packet %d %d\n", packets[next_packet_num].sequence, WINDOW_SIZE); 
          }
          else {
            printf("Error writing packet %d\n", packets[next_packet_num].sequence);
            error("Error writing to client"); 
          } 

          // [TODO]: Handle timing
          
          // Check if we are sending the final data packet
          if (packets[next_packet_num].type == TYPE_END_DATA) {
            all_sent = true; 
          }
          next_packet_num++;
        }

        // Handle client's ACKs
        int received_ack = handle_ack(sock_fd);
        printf("DEBUG: The received ack is %d. \n", received_ack); 

        if (received_ack > 0) {
 
          printf("DEBUG: The unacked packet array contains:\n"); 
          for (i=0; i < VECTOR_TOTAL(unacked_packets); i++) {
            printf("Index %d: %d, ", i, *(VECTOR_GET(unacked_packets, int*, i)));
          }
          printf("\n");
          // Determine if packet acked exists in unacked packet list:
          int index = VECTOR_EXISTS(unacked_packets, &received_ack);
          printf("DEBUG: The index of the packet with sequence %d is %d. \n", received_ack, index);
          if (index >= 0) {
            VECTOR_DELETE(unacked_packets, index);
          }
          else {
            printf("DEBUG: Packet with sequence number %d is not in the unacked vector\n", received_ack);
          }

          // Update the base to next unacked packet -- this is the first packet in the array
          base = (*(VECTOR_GET(unacked_packets, int*, 0)))/1024;
          printf("DEBUG: The new base is now: %d\n", base);

          // Update the unacked_packets vector with the new window
          for (i = base; i < base + window_num; i++) {
            int idx = VECTOR_EXISTS(unacked_packets, &packets[i].sequence); 
            if (idx == -1) {
              VECTOR_ADD(unacked_packets, &packets[i].sequence);
            }
          }

          // DEBUG: The new unacked_packet array contains: 
          printf("DEBUG: After updating the window, the unacked packet array contains:\n"); 
          for (i=0; i < VECTOR_TOTAL(unacked_packets); i++) {
            printf("Index %d: %d, ", i, *(VECTOR_GET(unacked_packets, int*, i)));
          }

          // [TODO]: Handle timing
        } 

      } // END OF RDT LOOP
    } // END OF REQUEST PACKET HANDLER
   


  } // END OF WHILE
  if (packets != NULL) {
    free(packets);
  }
}