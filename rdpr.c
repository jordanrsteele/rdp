/*
* rdpr.c
******************
****RECEIVER******
******************
*
*TESTING: ./rdpr 10.10.1.100 8082 received.dat
*
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


//define packet type
#define DAT 1
#define ACK 2
#define SYN 3
#define FIN 4
#define RST 5

#include "global.h"
# define BUFFER_SIZE 1024
#pragma pack(1)


//Global VARS
struct sockaddr_in source_addr;
socklen_t source_addr_size;

//another struct for dest ip
struct sockaddr_in dest_addr;
socklen_t dest_addr_size;


int expected_seq 	 = -1;
int num_packets_received = 0;
int window_size 	 = 10;
int num_syns 		 = 0;

//variables for logging
int total_bytes		= 0;
int uniq_bytes 	 	= 0;
int data_packs 	 	= 0;
int unique_packs 	= 0;
int total_syns	      	= 0;
int total_fins		= 0;
int total_rst		= 0;
int total_acks		= 0;
int total_rst_sent	= 0;

//flag is set when FIN received
int FIN_FLAG = 0;

//user inputs
FILE* fp;
char* source_ip;
int source_port;
char* dest_ip;
int dest_port;
//file pointer
char* file_name;
//time vals - start, stop
struct timeval start, stop;

//
int packet_log(packet log_packet, char event) {
	time_t t;	
	time(&t);
	struct tm* time_st = localtime(&t);
	char time_buf[17];
	strftime(time_buf, 16, "%H:%M:%S.%02u", time_st);
	char* packet_type;
	
	//convert type of packet
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

	//Log the packet send
	printf("%s %c %s  %s:%d %s:%d %s SEQ: %10d/     ACK: %10d    %d/%d\n", time_buf, event, packet_type,source_ip, source_port,
		dest_ip, dest_port, packet_type, log_packet.seqno, log_packet.ackno, log_packet.length, log_packet.size);

	 return 0;
	
	}

//prints final data log after transmission
int print_log() {
	//http://stackoverflow.com/questions/2150291/how-do-i-measure-a-time-interval-in-c
	
	gettimeofday(&stop, NULL);
	
	//time is in milliseconds
	double elapsedTime = (stop.tv_sec - start.tv_sec)*1000.0;
	elapsedTime += (stop.tv_usec - start.tv_usec)/1000.0;

	uniq_bytes = expected_seq;
	
	printf("total data bytes received:    %d\n", total_bytes);
	printf("unique data bytes received:   %d\n", expected_seq);
	printf("total data packets received   %d\n", data_packs);
	printf("unique data packets received: %d\n", unique_packs);
	printf("SYN packets received:         %d\n", total_syns);
	printf("FIN packets received:         %d\n", total_fins);
	printf("RST packets received:         %d\n", total_rst);
	printf("ACK packets sent:             %d\n", total_acks);
	printf("RST packets sent:             %d\n", total_rst_sent);
	printf("total time duration (second): %lf\n", elapsedTime/1000);
	
}

//packet handler, all packets will be dealt with here
int handle_packet(int sock, packet new_packet ) {
	
	dest_ip = inet_ntoa(dest_addr.sin_addr); //s_addr
	dest_port = htons(dest_addr.sin_port);
	total_bytes += new_packet.length;
	packet_log(new_packet, 'r');

	//received SYN packet
	if(new_packet.type == SYN){
		total_syns++;
		
		//start of transmission, syns received
		gettimeofday(&start, NULL);	
		expected_seq = new_packet.seqno + 1;
		push_ack(sock, dest_addr, dest_addr_size, new_packet.seqno + 1);
	}
	//received DAT packet
	if(new_packet.type == DAT) {
		data_packs++;
		if(new_packet.seqno == expected_seq){
			unique_packs++;
			expected_seq += fwrite(new_packet.data, 1, new_packet.length, fp);
		}
		push_ack(sock, dest_addr, dest_addr_size, expected_seq);
	}
	//recieved FIN packet
	if(new_packet.type == FIN) {	
		total_fins++;
		int i;
		for(i = 0; i < 20; i++) {
			push_ack(sock, dest_addr, dest_addr_size, new_packet.seqno + new_packet.length);
		}
		fclose(fp);

		print_log();
		exit(0);
	}
	//received RST packet	
	if(new_packet.type == RST) {
		total_rst++;
	
		push_rst(sock, dest_addr, dest_addr_size, new_packet.seqno);
		print_log();
		fclose(fp);

	}
			
	return 0;
}

int push_rst(int sock, struct sockaddr_in dest_addr, socklen_t dest_addr_size, int seqno) {
	
	char magicString[] = "CSC361";
	char newLine[] = "\n";
	char data[] = "THIS IS THE DATA SECTION of syn packet";

	//create a new packet
	packet rst_packet;
	//load contents
	memcpy(&rst_packet.magic, magicString, strlen(magicString));
	rst_packet.type         = ACK;
	rst_packet.seqno        = 0;
	rst_packet.ackno        = seqno;
	rst_packet.length       = 0;
	rst_packet.size         = 0;
	memcpy(&rst_packet.blank, newLine, strlen(newLine));
	memcpy(&rst_packet.data, data, strlen(data));

	char* buffer = malloc(sizeof(struct packet));
	memcpy(buffer, &rst_packet, sizeof(rst_packet));
	
	packet_log(rst_packet, 's');
	sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, dest_addr_size);
	
	return seqno;






}

//sends acknowledgment to sender ackno = seqno + length of data in packet
int push_ack(int sock, struct sockaddr_in dest_addr, socklen_t dest_addr_size, int seqno) {
	
	total_acks++;
	//printf("PUSHING ACK: \n");
	char magicString[] = "CSC361";
	char newLine[] = "\n";
	char data[] = "";

	//create a new packet
	packet ack_packet;
	//load contents
	memcpy(&ack_packet.magic, magicString, strlen(magicString));
	ack_packet.type         = ACK;
	ack_packet.seqno        = 0;
	ack_packet.ackno        = seqno;
	ack_packet.length       = 0;
	ack_packet.size         = 0;
	memcpy(&ack_packet.blank, newLine, strlen(newLine));
	memcpy(&ack_packet.data, data, strlen(data));

	char* buffer = malloc(sizeof(struct packet));
	memcpy(buffer, &ack_packet, sizeof(ack_packet));
	
	packet_log(ack_packet, 's');

	sendto(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, dest_addr_size);
	return seqno;

}


//RECEIVER CODE
int main(int argc, char*  argv[]) {
	
	//rdps receiver_ip receiver_port sender_file

	source_ip = argv[1];
	source_port = atoi(argv[2]);
	file_name = argv[3];
	
	fp = fopen(file_name, "wb+");

	//printf("source ip: %s   source port: %d\n", source_ip, source_port);

	int sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	ssize_t recsize;
	
	//set sock params
	memset(&source_addr, 0, sizeof(struct sockaddr_in));
	source_addr.sin_family = AF_INET;
	source_addr.sin_addr.s_addr = inet_addr(source_ip);
	source_addr.sin_port = htons(source_port);
	source_addr_size = sizeof(struct sockaddr_in);
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

	printf("rdpr is running on port %d \n", source_port);
	//printf("press q to quit...\n");
	char* buffer = malloc(BUFFER_SIZE);
	
	//coontinue looping and handle packets in handle_packet
	for(;;) {
		memset(buffer, '0', BUFFER_SIZE);
		//if recvfrom dest is null it is filled with sending ip
		recsize = recvfrom(sock, buffer, BUFFER_SIZE, 0, (struct sockaddr*)&dest_addr, &dest_addr_size);
		if(recsize < 0) {
			fprintf(stderr, "%s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}
		packet p1;
		memcpy(&p1, buffer, sizeof(struct packet));
		handle_packet(sock, p1);
				
	}
}
