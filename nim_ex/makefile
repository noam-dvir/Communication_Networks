all: nim nim-server 

clean:
	-rm nim nim-server nim.o nim-server.o
	
nim: nim.o
	gcc -o nim nim.c -g

nim.o: nim.c
	gcc -o nim.o -c nim.c -Wall -g

nim-server.o: nim-server.c
	gcc -o nim-server.o -c nim-server.c -Wall -g

nim-server: nim-server.o
	gcc -o nim-server nim-server.c -g
