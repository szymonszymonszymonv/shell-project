sshell: main.o
	gcc -o sshell main.o -Wall -lreadline
main.o: main.c
	gcc -c main.c