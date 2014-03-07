# Makefile for the mytftp-client program

mytftp-client:	mytftp-client.o
	gcc -o mytftp-client mytftp-client.o

mytftp-client.o:	mytftp-client.c Logging.h
	gcc -c mytftp-client.c