#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#define INIT_SIZE 10

/****************************** SERVER-SIDE API *******************************/

typedef struct func {
    char *name;
    rpc_handler handler;
} func_t;

struct rpc_server {
    /* Add variable(s) for server state */
    int port;
    int listen_sock_fd;
    int num_func, cur_size;
    func_t **functions;
};

struct rpc_handle {
    /* Add variable(s) for handle */
    int id;
};

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 */
rpc_server *rpc_init_server(int port) {

    rpc_server *srv = malloc(sizeof(*srv));
    if (!srv) return NULL;

    // int listen_sock_fd = -1;
    // struct sockaddr_in6 server_addr;

    // listen_sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	// if(listen_sock_fd == -1) return NULL;

    // int enable = 1;
    // if (setsockopt(listen_sock_fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
    //     perror("setsockopt");
    //     exit(EXIT_FAILURE);
    // }

    // server_addr.sin6_family = AF_INET6;
	// server_addr.sin6_addr = in6addr_any;
	// server_addr.sin6_port = htons(port);

    // if (bind(listen_sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
    //     return NULL;
    // }

    srv->port = port;
    // srv->listen_sock_fd = listen_sock_fd;

    srv->functions = malloc(INIT_SIZE * sizeof(*(srv->functions)));
    if (!srv->functions) return NULL;

    srv->num_func = 0;
    srv->cur_size = INIT_SIZE;

    return srv;

}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {

    if (srv == NULL || name == NULL || handler == NULL) return -1;

    // Ensure there's space in the array
    if (srv->num_func == srv->cur_size) {
        srv->cur_size *= 2;
        srv->functions = realloc(srv->functions,
                                 srv->cur_size * sizeof(*(srv->functions)));
        assert(srv->functions);
    }

    // Check for duplicate named functions
    int insert_idx;
    for (insert_idx = 0; insert_idx < srv->num_func; insert_idx++) {
        if (strcmp(srv->functions[insert_idx]->name, name) == 0) {
            break;
        }
    }

    srv->functions[insert_idx] = malloc(sizeof(func_t *));
    srv->functions[insert_idx]->name = strdup(name);
    assert(srv->functions[insert_idx]->name);
    srv->functions[insert_idx]->handler = handler;

    return srv->num_func++;

}

rpc_handle *rpc_find_func(rpc_server *srv, char *name) {

    int i;
    for (i = 0; i < srv->num_func; i++) {
        if (strcmp(srv->functions[i]->name, name) == 0) {
            rpc_handle *h = malloc(sizeof(rpc_handle *));
            h->id = i;
            return h;
        }
    }

    return NULL;

}

rpc_data *rpc_call_func(rpc_server *srv, rpc_handle *h, rpc_data *payload) {

    return srv->functions[h->id]->handler(payload);

}

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 */
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) return;    
    
    /* Prepare request */
    char left_operand = 1;
    char right_operand = 127;
    rpc_data request_data = { 
        .data1 = left_operand, .data2_len = 1, .data2 = &right_operand};


    rpc_handle *h = rpc_find_func(srv, "add2");
    rpc_data *response_data = rpc_call_func(srv, h, &request_data);

    /* Interpret response */
    assert(response_data->data2_len == 0);
    assert(response_data->data2 == NULL);
    printf("Result of adding %d and %d: %d\n", left_operand, right_operand,
            response_data->data1);

    rpc_data_free(response_data);

    // socklen_t client_addr_len;
    // char str_addr[INET6_ADDRSTRLEN];
    // int client_sock_fd = -1;
    // struct sockaddr_in6 client_addr;
    // char client_message[2000];

    // int queue_len = 10;
    // if (listen(srv->listen_sock_fd, queue_len) < 0) {
    //     close(srv->listen_sock_fd);
    //     return;
    // }

    // client_addr_len = sizeof(client_addr);

    // while(1) {

	// 	/* Do TCP handshake with client */
	// 	client_sock_fd = accept(srv->listen_sock_fd, 
	// 			(struct sockaddr*)&client_addr,
	// 			&client_addr_len);

	// 	if (client_sock_fd == -1) {
	// 		perror("accept()");
	// 		close(srv->listen_sock_fd);
	// 		return;
	// 	}

	// 	inet_ntop(AF_INET6, &(client_addr.sin6_addr), str_addr, sizeof(str_addr));
 
	// 	/* Wait for data from client */
	// 	if (recv(client_sock_fd, client_message, 2000, 0) < 0) {
	// 		perror("recv()");
	// 		close(client_sock_fd);
	// 		continue;
	// 	}
 
	// 	/* Send response to client */
	// 	if (send(client_sock_fd, client_message, strlen(client_message), 0) < 0) {
	// 		perror("send()");
	// 		close(client_sock_fd);
	// 		continue;
	// 	}
 
	// 	/* Do TCP teardown */
	// 	if (close(client_sock_fd) < 0) {
	// 		perror("close()");
	// 		client_sock_fd = -1;
	// 	}
 
	// 	printf("Connection closed\n");

	// }

    return;

}

/****************************** CLIENT-SIDE API *******************************/

struct rpc_client {
    /* Add variable(s) for client state */
    char *srv_addr;
    int port;
    int sock_fd;
};

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 */
rpc_client *rpc_init_client(char *addr, int port) {

    if (addr == NULL) return NULL;

    rpc_client *cl = malloc(sizeof(rpc_client));
    if (!cl) return NULL;

    // int sock_fd = -1;
	// struct sockaddr_in6 server_addr;
    
    // sock_fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	// if (sock_fd == -1) {
	// 	perror("socket()");
	// 	return NULL;
	// }

    // server_addr.sin6_family = AF_INET6;
	// inet_pton(AF_INET6, addr, &server_addr.sin6_addr);
	// server_addr.sin6_port = htons(port);

	// if (connect(sock_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
	// 	perror("connect()");
	// 	close(sock_fd);
	// 	return NULL;
	// }

    cl->srv_addr = addr;
    cl->port = port;
    // cl->sock_fd = sock_fd;

    return cl;

}

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 */
rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (cl == NULL || name == NULL) return NULL;

    // char *message = "hello world";

    // /* Send data to server */
	// if (send(cl->sock_fd, message, strlen(message), 0) < 0) {
	// 	perror("send");
	// 	close(cl->sock_fd);
	// 	return NULL;
	// }
 
	// /* Wait for data from server */
	// if (recv(cl->sock_fd, message, 2000, 0) < 0) {
	// 	perror("recv()");
	// 	close(cl->sock_fd);
	// 	return NULL;
	// }

    return NULL;

}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    if (cl == NULL || h == NULL || payload == NULL) return NULL;

    return NULL;

}

void rpc_close_client(rpc_client *cl) {

    if (cl == NULL) return;

	// if (close(cl->sock_fd) < 0) {
	// 	perror("close()");
	// 	return;
	// }

    free(cl);

    return;

}

/********************************* SHARED API *********************************/

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}
