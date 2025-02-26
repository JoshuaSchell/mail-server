CC=gcc
CFLAGS=-Wall -g 
LDFLAGS=-lpq -lcurl 

all: email-sender

email-sender: email-sender.c
	$(CC) $(CFLAGS) -o email-sender email-sender.c $(LDFLAGS)

clean:
	rm -f email-sender
