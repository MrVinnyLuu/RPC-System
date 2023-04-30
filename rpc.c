#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

/*********************************** SERVER ***********************************/

struct rpc_server {
    /* Add variable(s) for server state */
    int port;
    int num_func;
    char *func_name;
    rpc_handler func_handler;
};

rpc_server *rpc_init_server(int port) {

    rpc_server *srv = malloc(sizeof(rpc_server));
    if (!srv) return NULL;

    srv->port = port;
    srv->num_func = 0;

    return srv;

}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {

    if (srv == NULL || name == NULL || handler == NULL) return -1;

    srv->func_name = name;
    srv->func_handler = handler;

    return srv->num_func++;

}

/* Adapted from https://www.binarytides.com/server-client-example-c-sockets-linux/ */
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) return;

    // struct sockaddr_in server, client;
    // char client_message[2000];

    // int socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    // if (socket_desc == -1) return;

    // server.sin_family = AF_INET;
    // server.sin_addr.s_addr = INADDR_ANY;
    // server.sin_port = htons(8888);

    // if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
    //     return;
    // }

    // listen(socket_desc, 3);

    // int c = sizeof(struct sockaddr_in);

    // int client_sock =
    //         accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
    // if (client_sock < 0) return;

    // int read_size;
    // while ((read_size = recv(client_sock, client_message, 2000, 0)) > 0) {

    //     write(client_sock, client_message, strlen(client_message));

    // }
    
    return;

}

/*********************************** CLIENT ***********************************/

struct rpc_client {
    /* Add variable(s) for client state */
    char *srv_addr;
    int port;
};

struct rpc_handle {
    /* Add variable(s) for handle */
};

rpc_client *rpc_init_client(char *addr, int port) {

    if (addr == NULL) return NULL;

    rpc_client *cl = malloc(sizeof(rpc_client));
    if (!cl) return NULL;

    cl->srv_addr = addr;
    cl->port = port;

    return cl;

}

rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (cl == NULL || name == NULL) return NULL;



    return NULL;

}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    if (cl == NULL || h == NULL || payload == NULL) return NULL;

    return NULL;

}

void rpc_close_client(rpc_client *cl) {

    if (cl == NULL) return;

    free(cl);

    return;

}

/*********************************** SHARED ***********************************/

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
