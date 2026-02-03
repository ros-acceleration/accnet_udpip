CC = gcc
CFLAGS = -Wall -Wextra -I/usr/include/xrt
LDFLAGS = -lxrt_coreutil -lm

all: main lib

main:
	$(CC) $(CFLAGS) udriver.c main.c $(LDFLAGS) -o main

lib:
	$(CC) -shared -fPIC $(CFLAGS) -DTRACE -DLOG udriver.c socket.c -o libsock.so $(LDFLAGS)

lib-prod:
	$(CC) -shared -fPIC $(CFLAGS) udriver.c socket.c -o libsock.so $(LDFLAGS)
	
install:
	sudo cp libsock.so /lib/libsock.so

clean:
	rm -f main
	rm -f libsock.so
