CC      = gcc
CFLAGS  = -Wall -Wextra -O2

all: sender receiver

sender: sender.c protocol.c protocol.h
	$(CC) $(CFLAGS) -o sender sender.c protocol.c

receiver: receiver.c protocol.c protocol.h
	$(CC) $(CFLAGS) -o receiver receiver.c protocol.c

clean:
	rm -f sender receiver