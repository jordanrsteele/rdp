#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MAX_DATA_BYTES 900
#define BUFFER_SIZE 1024 
#define MAX_PACKET_SIZE 1024


//define packet type
#define DAT 1
#define ACK 2
#define SYN 3
#define FIN 4
#define RST 5


enum states {
	HANDSHAKE,
	SENDING,
	RESET,
	EXIT
};




//GLOBAL.H FILE
typedef struct packet {
	char  magic[12];
	int   type;
	int   seqno;
	int   ackno;
	int   length;
	int   size;
	char  blank[4];
	char  data[972];
	struct timeval timeout;

} packet;





//pushes a syn message to receiver for 3 way handshake
//int push_syn(int sock, struct sockaddr_in dest_addr, socklen_t dest_addr_size);

//int push_ack(int sock, struct sockaddr_in dest_addr, socklen_t dest_addr_size, int size, int seqnum);


//returns packet contents in a string	
//char* packet_to_string(packet *source);	


//packet string_to_packet(char* buff);








