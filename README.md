# RPC System

Project 2 for COMP30023_2023_SM1 Computer Systems.

"In this project, you will be building a custom RPC system that allows computations to be split seamlessly between multiple computers. This system may differ from standard RPC systems, but the underlying principles of RPC will still apply. [...] You must write your own RPC code, without using existing RPC libraries."

## Running the Project

- Download the repository
- Run `make`
- In one terminal run `./rpc_server < conf`, where conf is a "server.in" file
  - E.g: `./rpc_server < cases/1+1/server.in`
- In another terminal run `./rpc_client < conf`, where conf is a corresponding "client.in" file
  - E.g: `./rpc_server < cases/1+1/client.in`
