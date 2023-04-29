#include "rpc.h"
#include <stdlib.h>

/*********************************** SERVER ***********************************/

struct rpc_server {
    /* Add variable(s) for server state */
};

rpc_server *rpc_init_server(int port) {
    return NULL;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {
    if (srv == NULL || name == NULL || handler == NULL) return -1;

    return -1;
}

void rpc_serve_all(rpc_server *srv) {
    if (srv == NULL) return;

    return;
}

/*********************************** CLIENT ***********************************/

struct rpc_client {
    /* Add variable(s) for client state */
};

struct rpc_handle {
    /* Add variable(s) for handle */
};

rpc_client *rpc_init_client(char *addr, int port) {
    if (addr == NULL) return NULL;

    return NULL;
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
