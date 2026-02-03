CC = gcc
CFLAGS = -Wall -Wextra

TARGETS = rawsend rawrecv send recv echo-back-pl echo-back-ps

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm -f $(TARGETS)