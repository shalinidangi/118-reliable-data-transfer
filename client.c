#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>     
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <sys/time.h>

#define BUFSIZE 1024

void error(char *msg) {
    perror(msg);
    exit(0);
}

// Function that sends an acknowledgment to the server
void send_ACK(int ack_num, int sockfd, struct sockaddr_in serv_addr)
{
    // TODO
    return;
}

int main(int argc, char *argv[])
{
    // Example UDP client:
        // https://www.cs.cmu.edu/afs/cs/academic/class/15213-f99/www/class26/udpclient.c

    // Initialize variables
    int sockfd, portno, n, result;
    char* filename;
    FILE* f;

    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char* hostname;
    char buf[BUFSIZE];

    // Parse command line arguments
        // hostname
        // port number
        // filename
        // etc.
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

    
    // TODO: Create the request
        // Define a packet struct

    // REMOVE: temporary message from user for testing purposes
    // replace with actual server request
    bzero(buf, BUFSIZE);
    printf("Please enter msg: ");
    fgets(buf, BUFSIZE, stdin);

    // TODO: Send the request

    // REMOVE: Send temporary message to the server
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, buf, strlen(buf), 0, &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");

    // TODO: Start timer for timeout

    // TODO: Create a copy of the file for the client 
    // in order to detect corruption


    // TODO: Wait for response from server
    while (1) {

        // If there is a timeout, resend the packet

        // Otherwise, wait for/handle response

        // Check for corruption

        // REMOVE: temporary break
        break;
    }

    // REMOVE: Print server's reply
    n = recvfrom(sockfd, buf, strlen(buf), 0, &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");
    printf("Echo from server: %s", buf);
    return 0; 

    // Close the socket
    close(sockfd);

    // TODO: Close the client's copy of the file
    //fclose(f);
    //free(filecopy);
    return 0;
}