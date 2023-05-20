# This Makefile is adapted from the Makefile provided in COMP20003

# define C compiler & flags
CC = gcc
CFLAGS = -Wall -g
# define libraries to be linked
LIB = 

# define sets of header source files and object files
RPC_SYSTEM = rpc.o

SRC_SERVER = artifacts/server.a
SRC_CLIENT = artifacts/client.a
OBJ_SERVER = $(SRC_SERVER:.c=.o)
OBJ_CLIENT = $(SRC_CLIENT:.c=.o)
EXE_SERVER = rpc-server
EXE_CLIENT = rpc-client

all: $(RPC_SYSTEM) $(EXE_SERVER) $(EXE_CLIENT)

$(RPC_SYSTEM): rpc.c rpc.h
	$(CC) $(CFLAGS) -c -o $@ $<

RPC_SYSTEM_A=rpc.a
$(RPC_SYSTEM_A): rpc.o
	ar rcs $(RPC_SYSTEM_A) $(RPC_SYSTEM)

$(EXE_SERVER): $(OBJ_SERVER)
	$(CC) $(CFLAGS) -o $(EXE_SERVER) $(RPC_SYSTEM) $(OBJ_SERVER) $(LIB)

$(EXE_CLIENT): $(OBJ_CLIENT)
	$(CC) $(CFLAGS) -o $(EXE_CLIENT) $(RPC_SYSTEM) $(OBJ_CLIENT) $(LIB)

clean: 
	rm -f $(RPC_SYSTEM) $(EXE_SERVER) $(EXE_CLIENT)