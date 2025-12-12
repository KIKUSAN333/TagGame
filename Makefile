# Which compiler
CC=gcc

# Compiler Options for development
CFLAGS=-Wall

all:				tagServer tagClient

tagServer:	tagServer.c tagGame.o
						$(CC) $(CFLAGS) -o tagServer tagServer.c tagGame.o snet.a -lcurses

tagClient:	tagClient.c tagGame.o
						$(CC) $(CFLAGS) -o tagClient tagClient.c tagGame.o snet.a -lcurses

tagGame.o:	tagGame.c tagGame.h
						$(CC) $(CFLAGS) -c tagGame.c

clean:
						rm -f tagServer tagClient *.o
