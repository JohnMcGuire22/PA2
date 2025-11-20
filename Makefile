CC = gcc
CFLAGS = -std=gnu11 -Wall -Wextra -g -pthread

SRCS = chash.c hash.c rwlog.c parsing.c reentrant_threads.c
OBJS = $(SRCS:.c=.o)

all: chash

chash: $(OBJS)
	$(CC) $(CFLAGS) -o chash $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) chash hash.log

.PHONY: all clean
