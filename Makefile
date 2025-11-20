CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -pthread -g

OBJS = chash.o hash.o rwlog.o

all: chash

chash: $(OBJS)
	$(CC) $(CFLAGS) -o chash $(OBJS)

chash.o: chash.c hash.h rwlog.h
	$(CC) $(CFLAGS) -c chash.c

hash.o: hash.c hash.h rwlog.h
	$(CC) $(CFLAGS) -c hash.c

rwlog.o: rwlog.c rwlog.h
	$(CC) $(CFLAGS) -c rwlog.c

clean:
	rm -f *.o chash hash.log

.PHONY: all clean
