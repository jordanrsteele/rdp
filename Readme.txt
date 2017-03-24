Jordan Steele
V00733362

March 10 /2017





So far rdps and rdpr has only established connection management. 

How to run
	
	//this compiles the files
	run:  make

	//the receiver must be started first
	./rdpr 10.10.1.100 8080 received.dat

	
	//the sender 
	./rdps 192.168.1.100 8080 10.10.1.100 8080 sent.dat
	





Design Questions

1).

typedef struct packet {
	char  magic[12];
	int  type;
	int  seqno;
	int  ackno;
	int  length;
	int  size;
	char  blank[4];
	char  data[900]
} packet;



2).
During the handshake state the sender uses an infinite loop and select
along with a function called "push_syn" that returns the initial sequence
number. A SYN request is sent to the receiver and the sender waits 
for an ACK packet back. If no response after 2 seconds then another 
SYN request is sent. Initial sequnce number is chosen randomly.


3).
Both the sender and receiver will have a window size of N.
After each received message on the receiver side an ACK packet will be send back 
with window size N-1 telling the sender how many more packets it can handle in its window.
The greatest ACK number sent back to the sender will be the bottom of the window 
because we know all previous packets have been received.

The max payload of a packet is 900 bytes. 900 bytes will be read into a packet and sent
to receiver. The sequence num of the packet will tell the receiver which byte of the 
total file has been received where the receiver can write to file accordingly.
This will be done with fseek, fwrite and fread. File pointers make this easier.

The buffer is easily copied to packet using memcpy. The struct is a set number of bytes.
the packet can also be copied to a buffer easily also using memcpy



4).
Each packet sent will be time stamped and held in a list. After the window size of sent 
to the receiver the list will be checked to see if any of the packets have timed out.
If a timeout occurs then the packet will be resent and time stamped again.
One timer per window.
The timeouts will ensure reliable transfer. 
If a rst message is sent from the receiver to the sender then the connection will be dropped.


5).
What are the errors that can happen when a socket is non binding.



