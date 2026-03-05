CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -fno-builtin

PROTO_DIR    = protocol_pasta
SENDER_DIR   = sender_teste
RECEIVER_DIR = receiver_teste

all: sender receiver

sender: $(SENDER_DIR)/sender.c $(PROTO_DIR)/protocol.c $(PROTO_DIR)/protocol.h
	$(CC) $(CFLAGS) -I$(PROTO_DIR) -o sender $(SENDER_DIR)/sender.c $(PROTO_DIR)/protocol.c

receiver: $(RECEIVER_DIR)/receiver.c $(PROTO_DIR)/protocol.c $(PROTO_DIR)/protocol.h
	$(CC) $(CFLAGS) -I$(PROTO_DIR) -o receiver $(RECEIVER_DIR)/receiver.c $(PROTO_DIR)/protocol.c

clean:
	rm -f sender receiver