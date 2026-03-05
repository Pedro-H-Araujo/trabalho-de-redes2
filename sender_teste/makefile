CC     = gcc
CFLAGS = -Wall -Wextra -O2 -fno-builtin
PROTO  = ../protocol_pasta

all: sender

sender: sender.c $(PROTO)/protocol.c $(PROTO)/protocol.h
	$(CC) $(CFLAGS) -I$(PROTO) -o sender sender.c $(PROTO)/protocol.c

clean:
	rm -f sender
