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
#include <pthread.h>

#include "packet.h"
#include "vector.h"

#define MAX_BUFFER_SIZE 4096

/**
 * GLOBALS 
 */
int i; // Use for loops
int next_packet_num = 0; 
int base = 0; 
bool all_sent = false;
bool successful_transmission = false;  
bool sending_in_progress = false;
int window_num = WINDOW_SIZE / PACKET_SIZE;
struct Packet* packets = NULL;
int n_packets;

int sock_fd;
struct sockaddr_in serv_addr, client_addr;
unsigned int cli_len;

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

/**
 * This thread handles retransmission of packets 
 * and keeps track of each packets time
 */
void* timeout_check(void* dummy_arg) {
  int k;
  while (1) {
    if (sending_in_progress) {
      printf("inside the thread\n");
      time_t curr_time = time(NULL);
      // Check each packet in the current window
      for ( k = base; k < base + window_num; k++) {
        double time_diff = difftime(curr_time, packets[k].timestamp);
        if ((time_diff > 0.5) && !(packets[k].acked)) {
        	
          printf("[RETRANSMISSION] Packet %d must be retransmitted!\n", packets[k].sequence);
          
          if (sendto(sock_fd, &packets[k], sizeof(struct Packet), 0, 
             (struct sockaddr *) &client_addr, cli_len) > 0 ) {
            printf("Sending packet %d %d Retransmission\n", packets[k].sequence, WINDOW_SIZE);
            packets[k].timestamp = time(NULL);
          }

          else {
            error("Error retransmitting\n"); 
          }
        }
      } // END OF FOR
    }
  } // END OF WHILE
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
    data_packet.acked = false; 
    // Set last data packet
    if (i == num_packets - 1) {
      data_packet.type = TYPE_END_DATA; 
    }
    else {
      data_packet.type = TYPE_DATA; 
    }

    packets[i] = data_packet;
  }

  if (PRINT_DATA) {
    printf("DEBUG: Content of packet array: \n");
    print_packet_array(packets, num_packets);
  }
   
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
  
  int port, recv_len, yes = 1; 
  
  struct hostent *hostp; // Client host info
  char *hostaddrp; // Host address string
  
  bool established_connection = false; // used for handshake 

  
  FILE * f; 
   

  pthread_t thread_id; 

  
  
  
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

  // Start the timer thread
  pthread_create(&thread_id, NULL, &timeout_check, NULL);

  while(1) {
    printf("Waiting for incoming connections...\n"); 
    struct Packet received_packet; // Packet received from client
    // Initial time out value: A really long value
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
      error("ERROR setsockopt() failed");
    }

    // Receive a packet from client  
    printf("before recv_len\n");
    recv_len = recvfrom(sock_fd, &received_packet, sizeof(struct Packet), 0,
                       (struct sockaddr *) &client_addr, &cli_len);
    
    if (recv_len < 0) {
      continue;
    }

    printf("after recv len\n");
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
      
      sending_in_progress = true;
      // Send out packets by window
      // [TODO]: Move this to a function 
       

      // Initialize unacked_packets vector
      VECTOR_INIT(unacked_packets); 
      for (i = 0; i < base + window_num; i++) {
        VECTOR_ADD(unacked_packets, &packets[i].sequence); 
        printf("DEBUG: Initializing the vector array with sequence value - %d\n", *(VECTOR_GET(unacked_packets, int*, i)));  
      } 

      all_sent = false;
      while (all_sent == false || base < next_packet_num) {

        // Send the current packets in the window
        while (all_sent == false && next_packet_num < base + window_num) {
          
          // Send packet
          if (sendto(sock_fd, &packets[next_packet_num], sizeof(struct Packet), 0, 
             (struct sockaddr *) &client_addr, cli_len) > 0 ) {
            packets[next_packet_num].timestamp = time(NULL);
            printf("DEBUG: The timestamp for packet %d is %ld\n", packets[next_packet_num].sequence, packets[next_packet_num].timestamp);
            printf("Sending packet %d %d\n", packets[next_packet_num].sequence, WINDOW_SIZE); 
          }
          else {
            printf("Error writing packet %d\n", packets[next_packet_num].sequence);
            error("Error writing to client"); 
        } // END OF SEND LOOP 

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
            
            // Set the packet as acked in the packets array. We'll only look within the current window range, 
            // As anything out of it is acked already
            for (i = base; i < base + window_num; i++) {
              if (packets[i].sequence == received_ack) {
                packets[i].acked = true;

                // Check if we successfully received the ack for 
                // the last data packet  
                if (packets[i].type == TYPE_END_DATA) {
                  successful_transmission = true;
                }
              }
            }

            VECTOR_DELETE(unacked_packets, index);
            // Update the base to next unacked packet 
            // If there are still unacked packets - this is the first packet in the array
            if (VECTOR_TOTAL(unacked_packets) != 0) {
              base = (*(VECTOR_GET(unacked_packets, int*, 0)))/1024;
            } 
            // If there are no more unacked packets left in the window, jump to a whole new window
            else {
              base += window_num;
            }
            
            printf("DEBUG: The new base is now: %d\n", base);
            printf("DEBUG: The new window is from BASE: %d to %d\n", packets[base].sequence, packets[base+window_num].sequence);
            // Update the unacked_packets vector with the new window
            for (i = base; i < base + window_num; i++) {
              int idx = VECTOR_EXISTS(unacked_packets, &packets[i].sequence); 
              if (idx == -1) {
                if ((!packets[i].acked) && (VECTOR_TOTAL(unacked_packets) < 5)) {
                	printf("i is: %d\n", i);
                  VECTOR_ADD(unacked_packets, &packets[i].sequence);
                }
                printf("DEBUG: Packet %d is already acked!\n", packets[i].sequence);
              }
            }

            // DEBUG: The new unacked_packet array contains: 
            printf("DEBUG: After updating the window, the unacked packet array contains:\n"); 
            for (i=0; i < VECTOR_TOTAL(unacked_packets); i++) {
              printf("Index %d: %d, ", i, *(VECTOR_GET(unacked_packets, int*, i)));
            }

            if (successful_transmission) {
              printf("Successfully transmitted file!\n");

              // Send a FIN packet to the client
              struct Packet fin_packet;
              fin_packet.sequence = 0; 
              fin_packet.type = TYPE_FIN;
              fin_packet.ack = received_packet.ack + 1; 
              if (sendto(sock_fd, &fin_packet, sizeof(struct Packet), 0, 
                        (struct sockaddr *) &client_addr, cli_len) > 0 ) {
                    printf("Sending packet %d %d FIN\n", fin_packet.sequence, WINDOW_SIZE);
              }
              else {
                printf("Error writing FIN packet\n"); 
              }

              // Wait for FIN_ACK 
              // Set timeout value of sock_fd to 500 ms
              tv.tv_sec = 0;
              tv.tv_usec = RETRANSMISSION_TIME_OUT * 1000;
              if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
                error("ERROR setsockopt() failed");
              }

              int run = 1;
              while(run) {
                struct Packet response; 
                if (recvfrom(sock_fd, &response, sizeof(response), 0, (struct sockaddr *) &client_addr, &cli_len) >= 0) {
                  if (response.type == TYPE_FIN_ACK) {
                    // Wait for timeout
                    if (recvfrom(sock_fd, &response, sizeof(response), 0, (struct sockaddr *) &client_addr, &cli_len) >= 0) {
                      printf("ERROR unexpected packet type received\n");
                    }
                    else {
                      // Timeout occured, close connection! 
                      printf("DEBUG: Closing connection, resetting all variables\n");
                      established_connection = false;
                      sending_in_progress = false; 
                      if (packets != NULL) {
                        free(packets);
                      }
                      packets = NULL;
                      next_packet_num = 0; 
                      base = 0; 
                      successful_transmission = false;  
                      
                      run = 0; // force break out of while loop
                    }
                    break;
                  }
                  else {
                    printf("ERROR unexpected packet type received\n");
                  }
                }
                else if (response.type == TYPE_ACK) {
                  printf("Received a late ack, ignoring\n");
                  continue;
                }
                else {
                  // Resend FIN
                  if (sendto(sock_fd, &fin_packet, sizeof(struct Packet), 0, 
                     (struct sockaddr *) &client_addr, cli_len) > 0 ) {
                    printf("Sending packet %d %d FIN\n", fin_packet.sequence, WINDOW_SIZE);
                  }
                  else {
                    printf("Error writing FIN packet after retransmission\n"); 
                  }
                }
              } // END OF FIN_ACK LOOP

              all_sent = true; // force out of while loop
              break;
            } // END OF SUCCESSFUL TRANSMISSION
          } // END OF HANDLING ACK INSIDE UNACKED VECTOR
          else {
            printf("DEBUG: Packet with sequence number %d is not in the unacked vector\n", received_ack);
          }
        } // END OF SUCCESSFULLY RECEIVING AN ACK 

      } // END OF RDT LOOP
    } // END OF REQUEST PACKET HANDLER

  } // END OF WHILE
  
}