/*
*rdps.c
***************
****SENDER*****
***************
*
*
*TESTING: ./rdps 192.168.1.100 8080 10.10.1.100 8080 sent.dat
*         rdps    send_ip     sPort  rec_ip  rec_port  sender_file
*/

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

#pragma pack(1)

//shared file
#include "global.h"



//define packet type
#define DAT 1
#define ACK 2
#define SYN 3
#define FIN 4
#define RST 5

#define MAX_PACKET_SIZE 1024
#define BUFFER_SIZE 1024
#define MAX_WINDOW_SIZE 10
#define MAX_DATA 900

#define TIME_OUT 30000
int window_size = MAX_WINDOW_SIZE;

//SENDER STRUCT
struct sockaddr_in source_addr;
socklen_t source_addr_size; 

//RECEIVER STRUCT
struct sockaddr_in dest_addr;
socklen_t dest_addr_size;


FILE* fp;


int sock;
int seqno;
int closing_ack = -1;

char* source_ip;
int source_port;
char* dest_ip;
int dest_port;
char* fileName;

int FIN_FLAG = 0;


int total_bytes_sent = 0;
 


packet* packetList[MAX_WINDOW_SIZE];


int packet_log(packet log_packet, char event) {
	time_t t;
    	time(&t);
      	struct tm* time_st = localtime(&t);
        char time_buf[17];
	strftime(time_buf, 16, "%H:%M:%S.%02u", time_st);
	char* packet_type;

	if(log_packet.type == 1)
		packet_type = "DAT";
	else if(log_packet.type == 2)
		packet_type = "ACK";
	else if(log_packet.type == 3)
		packet_type = "SYN";
	else if(log_packet.type == 4)
		packet_type = "FIN";
	else if(log_packet.type == 5)
		packet_type = "RST";	

	printf("%s %c %s  %s:%d %s:%d %s SEQ: %10d / ACK: %10d %d/%d\n", time_buf, event, packet_type,source_ip, source_port,
						dest_ip, dest_port, packet_type, log_packet.seqno, 
						log_packet.ackno, log_packet.length, log_packet.size);


	return 0;
}

int print_log() {

	//http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c	

	printf("total data bytes sent: 		%d\n", seqno);	
	printf("unique data bytes sent: 	%d\n", seqno);
	printf("total data packets sent:	%d\n", seqno);
	printf("unique data packets sent: 	%d\n", seqno);
	printf("SYN packets sent: 		%d\n", seqno);
	printf("FIN packets sent: 		%d\n", seqno);
	printf("RST packets sent: 		%d\n", seqno);
	printf("ACK packets received: 		%d\n", seqno);
	printf("RST packets received: 		%d\n", seqno);

}

int print_list() {

	int i = 0;
	while(i < MAX_WINDOW_SIZE) {
		
		if(packetList[i] != NULL){
			printf("LIST SEQNOS: %d\n", packetList[i]->seqno);
		}
		i++;
	}
	return 0;
}

void resend(packet* pack) {	

	char* buffer = malloc(BUFFER_SIZE);
	memset(buffer, '\0', BUFFER_SIZE);
	
	memcpy(buffer, pack, sizeof(struct packet));		
	sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, dest_addr_size);
	
	packet new_pack;
	memset(&new_pack, '\0', sizeof(struct packet));
	memcpy(&new_pack, pack, sizeof(struct packet));

	packet_log(new_pack, 'S');
	
	free(buffer);
	//return 0;
}

int update_list(int acknumber) {

	int i;
	for(i = 0;i < MAX_WINDOW_SIZE; i++) {
		if(packetList[i] != NULL) {
			//remove from list
			if(packetList[i]->seqno < acknumber) {
				free(packetList[i]);
				packetList[i] = NULL;
				
				//update window size
				window_size++;
			}
		}
	}
	return 0;
}

int is_empty() {
	int i;
	for(i = 0; i < MAX_WINDOW_SIZE; i++){

		if(packetList[i] != NULL){
			return -1;
		}
	}
	return 0;
}


//iterates through packetList checking for timed out packets
//resets timed out packages
int check_timeouts() {
	struct timeval now;
	struct timeval new_timeout;
	new_timeout.tv_sec = 0;
	new_timeout.tv_usec = TIME_OUT;
	gettimeofday(&now, NULL);
	
	timeradd(&now, &new_timeout, &new_timeout);
	
	int i;
	for(i = 0; i < MAX_WINDOW_SIZE; i++) {
		if(packetList[i] != NULL) {
			if( timercmp(&now, &packetList[i]->timeout, >)) {
				resend(packetList[i]);
				
				//reset timers timeout
				memcpy(&packetList[i]->timeout, &new_timeout, sizeof(struct timeval));

			}		
		}
	}
	return 0;
}

int insert_packet(packet pack, struct timeval time) {
	int i;
	for (i = 0; i < MAX_WINDOW_SIZE; i++) {
		if(packetList[i] == NULL) {
			packetList[i] = calloc( 1, sizeof(struct packet));
			memcpy(packetList[i], &pack, sizeof(struct packet));
			memcpy(&packetList[i]->timeout, &time, sizeof(struct timeval));
			
			return;			
		}	
	}
}


int initializeArr() {
	int i = 0;
	while(i < MAX_WINDOW_SIZE) {
		packetList[i] = NULL;
		i++;
	}
	return 0;
}

int fill_window(int sock, struct sockaddr_in dest_addr, socklen_t dest_addr_size) {
	//printf("FILL WINDOW\n");
	char magicString[] = "CSC361";
	char newLine[] = "\n";  

	packet data_packet;

	while(window_size > 0) {

		memset(&data_packet, '\0', sizeof(struct packet));
		//load contents
		//timer for packet
		struct timeval now;
		struct timeval adder;
		struct timeval timeout;
		adder.tv_sec = 0;
		adder.tv_usec = TIME_OUT;
		gettimeofday(&now, NULL);
		timeradd(&now, &adder, &timeout);

		memcpy(&data_packet.timeout, &timeout, sizeof(struct timeval));	
		
		memset(&data_packet, '\0', sizeof(struct packet));
		memcpy(&data_packet.magic, magicString, strlen(magicString));
		data_packet.type         = DAT;
		data_packet.seqno        = seqno;
		data_packet.ackno        = 0;
		data_packet.length       = 0;
		data_packet.size         = window_size;
		memcpy(&data_packet.blank, newLine, strlen(newLine));
		
		char* buffer = malloc(BUFFER_SIZE);
		memset(buffer, '\0', BUFFER_SIZE);
		
		seqno += (data_packet.length = fread(&data_packet.data, sizeof(char), MAX_DATA,fp));
		
		if(data_packet.length > 0) {
			memcpy(buffer, &data_packet, sizeof(struct packet));		
			sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, dest_addr_size);
			//printf("PACKET SENT\n");	
			packet_log(data_packet, 's');
			insert_packet(data_packet, timeout);
		} else {
			FIN_FLAG = 1;
			//push_fin(timeout);
		}
		
		free(buffer);
		window_size--;
	}
	return 0;
}
int push_fin(struct timeval time) {

	char magicString[] = "CSC361";
	char newLine[] = "\n";  

	packet fin_packet;

	//copy the time val
	memcpy(&fin_packet.timeout, &time, sizeof(struct timeval));
	memset(&fin_packet, '\0', sizeof(struct packet));
	memcpy(&fin_packet.magic, magicString, strlen(magicString));
	fin_packet.type         = FIN;
	fin_packet.seqno        = seqno;
	fin_packet.ackno        = 0;
	fin_packet.length       = 0;
	fin_packet.size         = window_size;
	memcpy(&fin_packet.blank, newLine, strlen(newLine));
			
	char* buffer = malloc(BUFFER_SIZE);
	memset(buffer, '\0', BUFFER_SIZE);
		
	memcpy(buffer, &fin_packet, sizeof(struct packet));		
	sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, dest_addr_size);
	
	//log the packet and insert into list
	packet_log(fin_packet, 's');

	insert_packet(fin_packet, time);
	packet_log(fin_packet, 's');	
	free(buffer);

	closing_ack = seqno;

	return 0;
}

//return sequence number
int push_syn(int socket, struct sockaddr_in dest_addr1, socklen_t dest_addr_size1) {
	//printf("PUSHING SYN: \n");
	char magicString[] = "CSC361";
	char newLine[] = "\n";
	char data[] = "THIS IS THE DATA SECTION of syn packet";
	seqno = 0;

	//create a new packet
	packet syn_packet;
	//load contents
	memcpy(&syn_packet.magic, magicString, strlen(magicString));
	syn_packet.type 	= SYN;
	syn_packet.seqno 	= 0;
	syn_packet.ackno 	= 0;
	syn_packet.length 	= 0;
	syn_packet.size 	= 0;
	memcpy(&syn_packet.blank, newLine, strlen(newLine));
	memcpy(&syn_packet.data, data, strlen(data));
	
	char* buff = calloc(1, BUFFER_SIZE);
	memcpy(buff, &syn_packet, sizeof(struct packet));
	
	sendto(socket, buff, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr1, dest_addr_size1);
	packet_log(syn_packet, 's');	
	free(buff);
	return seqno;
}


//SENDER CODE
int main(int argc, char*  argv[]) {

	
	time_t t;
    	time(&t);
      	struct tm* time_st = localtime(&t);
        char time_buf[17];
	strftime(time_buf, 16, "%H:%M:%S.%02u", time_st);
	

	initializeArr();
	
	//rdps sender_ip sender_port receiver_ip receiver_port sender_file
	if(argc != 6){
		printf("rdps sender_ip sender_port receiver_ip receiver_port sender_file");
		exit(-1);
	}

	source_ip = argv[1];
	source_port = atoi(argv[2]);
	dest_ip = argv[3];
	dest_port = atoi(argv[4]);
	fileName = argv[5];
	fp = fopen(fileName, "rb");

		
	//printf("sender ip: %s\n sender port: %d\n receiver ip: %s\n receiver port:%d\n filename%s\n", source_ip, source_port, dest_ip, dest_port, fileName);

	//create sock
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ssize_t recsize;
	

	//set source_addr
	//binds with port
	memset(&source_addr, 0, sizeof(struct sockaddr_in));
	source_addr.sin_family = AF_INET;
	source_addr.sin_addr.s_addr = inet_addr(source_ip);
	source_addr.sin_port = htons(source_port);
	source_addr_size = sizeof(struct sockaddr_in);


	//set dest_addr
	//recv v listens for this address
	memset(&dest_addr, 0, sizeof(struct sockaddr_in));
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_addr.s_addr = inet_addr(dest_ip);
	dest_addr.sin_port = htons(dest_port);
	dest_addr_size = sizeof(struct sockaddr_in);
	
	
	//set socket
	int opt = 1;
	if(-1 == setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		perror("Socket not set");
	}

	//bind socket
	if (-1 == bind(sock, (struct sockaddr *)&source_addr, sizeof(source_addr))) {
		perror("error bind failed");						
		close(sock);								
		exit(1);
	}
	


	printf("rdps is running on port %d \n", source_port);
	//printf("press q to quit...\n");

	//***************ESTABLISH HANDSHAKE*****************
	//stay in this loop till ack received
	//fcntl(sock, F_SETFL, O_NONBLOCK); 
	int start_seqno = 0;
	seqno = start_seqno;
	enum states state = HANDSHAKE;

	//char* buffer = malloc(BUFFER_SIZE);
	char buffer[1024];
	//memset(buffer, '\0', sizeof buffer +1);
	push_syn(sock, dest_addr, dest_addr_size);
	
	
	//select function lets socket wait for response
	int result = 0;
	fd_set readset;
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 30000;

	packet new_packet;
	while(1) {
		FD_ZERO(&readset);
		FD_SET(sock, &readset);
		result = select(sock + 1, &readset, NULL, NULL, &tv);	
		if (FD_ISSET(sock, &readset) && result > 0) {
		
			//wait for ack message from client
			memset(buffer, '\0', sizeof buffer +1);
			recsize = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, &dest_addr_size);
			
			if(recsize < 0) {
				fprintf(stderr, "%s\n", strerror(errno));
                        	exit(EXIT_FAILURE);
	                }				
			memcpy(&new_packet, buffer, recsize);
			if(new_packet.type == ACK) {
				state = SENDING;

				packet_log(new_packet, 'r');
				seqno = new_packet.ackno;	
				fill_window(sock, dest_addr, dest_addr_size);
				break;
			}
		}
		//found nothing send another
		push_syn(sock, dest_addr, dest_addr_size);
	}
	

	//set non blocking 
	fcntl(sock, F_SETFL, O_NONBLOCK);
	
	//check_timeouts();

	int count = 0;
	int index = 0;
	for(;;) {
		
		memset(buffer, '\0', sizeof buffer +1);
		recsize = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, &dest_addr_size);
		//we recieve a new message!
		if (recsize > 0){
			memset(&new_packet, '\0', sizeof(struct packet));	
			memcpy(&new_packet, buffer, recsize);			
			
			packet_log(new_packet, 'r');
			if(new_packet.type == ACK) {
				//update_list
				update_list(new_packet.ackno);									
			}
			
			//keep checking for closing ackno
			if(new_packet.ackno == closing_ack){

				printf("END OF TRANSMISSION...\n");
				fclose(fp);
				close(sock);
				exit(0);
			
			}

		}
		//if a packet is timed out then it will be resent 
		check_timeouts();
		if(window_size > 5) {
			fill_window(sock, dest_addr, dest_addr_size);
		}
		
		
		int count = is_empty();
		
		//if fin flag is set then add timer to fin packet
		if(FIN_FLAG = 1 && count == 0) {
			printf("FIN FLAG SET\n");
			struct timeval now;
			struct timeval adder;
			struct timeval timeout;
			adder.tv_sec = 0;
			adder.tv_usec = TIME_OUT;
			gettimeofday(&now, NULL);
			timeradd(&now, &adder, &timeout);
			push_fin(timeout);

		}



	}

	return 0;	
}


