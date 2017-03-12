/**
 * packet.h
 * High level representation of a packet 
 */

#define PACKET_SIZE 1024
#define PACKET_DATA_SIZE 1000
#define RETRANSMISSION_TIME_OUT 500

// PACKET TYPES
#define TYPE_REQUEST 0
#define TYPE_DATA 1
#define TYPE_ACK 2
#define TYPE_END_DATA 3

struct Packet {
  int sequence; 
  int type; 
  int length;
  char data[PACKET_DATA_SIZE];  
}