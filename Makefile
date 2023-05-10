# This Makefile is adapted from the Makefile provided in COMP20003

# define C compiler & flags
CC = gcc
CFLAGS = -Wall -g
# define libraries to be linked
LIB = 

# define sets of header source files and object files
SRC_SERVER = rpc.c server.c
SRC_CLIENT = rpc.c client.c

OBJ_SERVER = $(SRC_SERVER:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)

EXE_SERVER = server
EXE_CLIENT = client

$(EXE_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $(EXE_SERVER) $(OBJ_SERVER) $(LIB)

$(EXE_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $(EXE_CLIENT) $(OBJ_CLIENT) $(LIB)

RPC_SYSTEM = rpc.o

.PHONY: format all

all: $(RPC_SYSTEM) $(EXE_SERVER) $(EXE_CLIENT)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) $(CFLAGS) -c -o $@ $<

# RPC_SYSTEM_A=rpc.a
# $(RPC_SYSTEM_A): rpc.o
# 	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

format:
	clang-format -style=file -i *.c *.h

clean: 
	rm -f $(RPC_SYSTEM) $(OBJ_SERVER) $(OBJ_CLIENT) $(EXE_SERVER) $(EXE_CLIENT)