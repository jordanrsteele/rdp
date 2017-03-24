.phony all:
all: rdps rdpr 


stack:
	gcc -o rdps rdps.c -fno-stack-protector 
	gcc -o rdpr rdpr.c

rdps: rdps.c
	gcc -o rdps rdps.c 
rdpr: rdpr.c
	gcc -o rdpr rdpr.c 



run: 

	./rdpr 10.10.1.100 8080 received.dat& 
	./rdps 192.168.1.100 8080 10.10.1.100 8080 sent.dat 


PHONY clean:
clean:
	-rm -rf *.o *.exe
	-rm -rf ./rdps
	-rm -rf ./rdpr


