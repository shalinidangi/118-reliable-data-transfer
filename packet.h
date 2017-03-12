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

// DEBUG 
#define PRINT_DATA 0

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
    
    printf("Sequence: %d, Packet type: %s, Data length: %d\n", p.sequence, type, p.length);

    if (PRINT_DATA) {
      printf("Data: %s\n", p.data);  
    }
}

void print_packet_array(struct Packet* p, int size) {
  if (p == NULL) {
    printf("DEBUG: Packet array is null.\n");
    return; 
  }
  int i; 
  for (i = 0; i < size; i++) {
    print_packet(p[i]); 
  }
}