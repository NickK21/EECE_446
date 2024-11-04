# EECE-446-FA-2024 | Nick Kaplan | Halin Gailey

CC = gcc
CFLAGS = -Wall

all: peer

peer: peer.o
	$(CC) $(CFLAGS) -o peer peer.o
	\
peer.o: peer.c
	$(CC) $(CFLAGS) -c peer.c

clean:
	rm -rf peer.o peer

