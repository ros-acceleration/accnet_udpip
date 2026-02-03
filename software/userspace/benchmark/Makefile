CC = gcc
CFLAGS = -Wall -Wextra

TARGETS = benchmark user

all: $(TARGETS)

benchmark: benchmark.c
	$(CC) $(CFLAGS) benchmark.c -o benchmark

user: main.c
	$(CC) $(CFLAGS) main.c ../userspace/udriver.c -I../userspace -I/usr/include/xrt -lxrt_coreutil -lm -o user

clean:
	rm -f $(TARGETS)