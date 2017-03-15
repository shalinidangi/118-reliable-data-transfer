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

      int i; 
      for (i = 0; i < n_packets; i++) {
        packets[i].ack = received_packet.length; 
        if (sendto(sock_fd, &packets[i], sizeof(struct Packet), 0, 
                  (struct sockaddr *) &client_addr, cli_len) > 0 ) {
            printf("Sending packet %d %d\n", packets[i].sequence, WINDOW_SIZE);
        }
        else {
          printf("Error writing Packet #%d\n", packets[i].sequence); 
        }
      }
    } // END OF REQUEST PACKET HANDLER
   
  }
  
}