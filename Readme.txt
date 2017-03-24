Jordan Steele
V00733362

March 10 /2017



How to run
	
	//this compiles the files
	run:  make

	//the receiver must be started first
	./rdpr 10.10.1.100 8080 received.dat

	
	//the sender 
	./rdps 192.168.1.100 8080 10.10.1.100 8080 sent.dat
	
	
	sent.dat is the file you wish to send
	received.dat is the file which will be written too


Implementation features:

	rdpr.c and rdps.c simulates a reliable data protocol. Go back N is chosen for the
	design. On the sender side select is used for the initial handshake before 
	any data packet is sent. Once in the main loop no more than 10 sent packets will be acknowledges 
	at any given time. After each packet is sent it is put into a list with a timestamp.
	If any packet has timed out it is immediately resent to the receiver and re-stamped.
	If an Ack is received then any packet with a sequence number less than the received ack number is 
	deleted from the list because we know we have the data up to the acknumber.
	On the Receiver side only the next packet in the sequence is accepted. Anything else is dropped.
	In the closing connection select is used on 
	


