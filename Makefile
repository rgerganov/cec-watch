CFLAGS += -Wall -std=c99 -D _BSD_SOURCE

all: cec-watch

cec-watch: cec-watch.c
	$(CC) cec-watch.c -o cec-watch $(CFLAGS)

clean:
	rm -f cec-watch *.o

