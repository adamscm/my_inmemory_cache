#!/bin/sh
CCFLAGS = -g3 -Wall -o0 

.PHONY:clean
all:myInmemoryCache test_client
myInmemoryCache.o:myInmemoryCache.c
	gcc -g -o myInmemoryCache.o -c myInmemoryCache.c
myInmemoryCache:myInmemoryCache.o
	gcc -g -o myInmemoryCache myInmemoryCache.o -lpthread
test_client.o:test_client.c
	gcc -g -o test_client.o -c test_client.c
test_client:test_client.o
	gcc -g -o test_client test_client.o -lpthread
clean:
	rm *.o myInmemoryCache test_client
