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

#define MAX_BUFFER_SIZE 4096

/*
 * error - wrapper for perror
 */
void error(char *msg) {
  perror(msg);
  exit(1);
}

int main(int argc, char *argv[]) {
  
  /**
   * Code based on examples from: 
   * https://www.tutorialspoint.com/unix_sockets/socket_server_example.htm
   * http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#intro
   * https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpserver.c
   * CS118 Discussion Slides
   */
  
  int sock_fd, port, yes = 1; 
  unsigned int cli_len;
  struct sockaddr_in serv_addr, client_addr;
  char buf[MAX_BUFFER_SIZE]; /* message buf */
  struct hostent *hostp; /* client host info */
  char *hostaddrp; /* dotted decimal host addr string */
  int n;

  // Parse port number 
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);
  }
  port = atoi(argv[1]);

  // Open socket connection
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
    
  
    // recvfrom: receive a UDP datagram from a client
     
    bzero(buf, MAX_BUFFER_SIZE);
    n = recvfrom(sock_fd, buf, MAX_BUFFER_SIZE, 0,
     (struct sockaddr *) &client_addr, &cli_len);
    if (n < 0)
      error("ERROR in recvfrom");

   
    // gethostbyaddr: determine who sent the datagram
     
    hostp = gethostbyaddr((const char *)&client_addr.sin_addr.s_addr, 
        sizeof(client_addr.sin_addr.s_addr), AF_INET);
    if (hostp == NULL)
      error("ERROR on gethostbyaddr");
    hostaddrp = inet_ntoa(client_addr.sin_addr);
    if (hostaddrp == NULL)
      error("ERROR on inet_ntoa\n");
    printf("server received datagram from %s (%s)\n", 
     hostp->h_name, hostaddrp);
    printf("server received %d/%d bytes: %s\n", strlen(buf), n, buf);
    
   
    // sendto: echo the input back to the client 
  
    n = sendto(sock_fd, buf, strlen(buf), 0, 
         (struct sockaddr *) &client_addr, cli_len);
    if (n < 0) 
      error("ERROR in sendto");
  }
  
}