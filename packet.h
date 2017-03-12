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
};

void print_packet(struct Packet p) {

	char* type;

    if (p.type == TYPE_REQUEST) 
        type = "TYPE_REQUEST";
    else if (p.type == TYPE_DATA)
        type = "TYPE_DATA";
    else if (p.type == TYPE_ACK)
        type = "TYPE_ACK";
    else if (p.type == TYPE_END_DATA)
        type = "TYPE_END_DATA";
    else
        type = "UNKNOWN";
    
    printf("Packet type: %s, Sequence: %d, Data length: %d\n", type, p.sequence, p.length);

    printf("Data: %s\n", p.data);
}