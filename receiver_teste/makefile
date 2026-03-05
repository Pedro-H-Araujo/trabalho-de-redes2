CC     = gcc
CFLAGS = -Wall -Wextra -O2 -fno-builtin
PROTO  = ../protocol_pasta

all: receiver

receiver: receiver.c $(PROTO)/protocol.c $(PROTO)/protocol.h
	$(CC) $(CFLAGS) -I$(PROTO) -o receiver receiver.c $(PROTO)/protocol.c

clean:
	rm -f receiver
