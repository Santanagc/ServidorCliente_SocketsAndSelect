all:
	gcc -Wall equipment.c -o equipment
	gcc -Wall server.c -o server
clean:
	rm  equipment server 