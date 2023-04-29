# This Makefile is adapted from the Makefile provided in COMP20003

# define C compiler & flags
CC = gcc
CFLAGS = -Wall -g
# define libraries to be linked
LIB = 

RPC_SYSTEM = rpc.o

.PHONY: format all

all: $(RPC_SYSTEM)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) $(CFLAGS) -c -o $@ $<

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
# 	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

format:
	clang-format -style=file -i *.c *.h

clean:
	rm -f $(RPC_SYSTEM)