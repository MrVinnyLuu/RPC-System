#define _POSIX_C_SOURCE 200112L

#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>

#define INIT_SIZE 10

/********************************* SHARED API *********************************/

rpc_data *rpc_str_to_data(char *s) {

    rpc_data *d = malloc(sizeof(rpc_data *));
    d->data1 = atoi(strtok(s, ","));
    d->data2_len = atoi(strtok(s, ","));
    d->data2 = strtok(s, ",");

    return d;

}

char *rpc_data_to_str(rpc_data *d) {

    char *s = (char*)malloc(sizeof(d));

    memcpy(s, (const unsigned char*)&d, sizeof(d));

    for(int i=0;i<sizeof(d);i++) printf("%02X ",s[i]);
    puts("***********************");
    puts(s);

    return s;

}

void rpc_print_data(rpc_data *d) {
    printf("%d,%ld,%p", d->data1, d->data2_len, d->data2);
}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) {
        return;
    }
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}


/****************************** SERVER-SIDE API *******************************/

typedef struct func {
    char *name;
    rpc_handler handler;
} func_t;

struct rpc_server {
    /* Add variable(s) for server state */
    // int port;
    int sock_fd;
    int num_func, cur_size;
    func_t **functions;
};

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
rpc_server *rpc_init_server(int port) {

    // Initialise server side 3 tuple
    int sock_fd = -1;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6; // IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;
    char service[5];
    sprintf(service, "%d", port);
    if (getaddrinfo(NULL, service, &hints, &servinfo) < 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}

    sock_fd = socket(servinfo->ai_family, servinfo->ai_socktype,
                     servinfo->ai_protocol);
	if(sock_fd < 0) {
        perror("socket");
        return NULL;
    }

    int re = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    if (bind(sock_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        return NULL;
    }

    freeaddrinfo(servinfo);

    // Initialise server structure
    rpc_server *srv = malloc(sizeof(rpc_server *));
    if (!srv) {
        perror("malloc");
        return NULL;
    }

    // srv->port = port;
    srv->sock_fd = sock_fd;

    srv->functions = malloc(INIT_SIZE * sizeof(*(srv->functions)));
    if (!srv->functions) {
        perror("malloc");
        return NULL;
    }

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
    int i;
    for (i = 0; i < srv->num_func; i++) {
        if (strcmp(srv->functions[i]->name, name) == 0) {
            break;
        }
    }

    srv->functions[i] = malloc(sizeof(func_t *));
    assert(srv->functions[i]);

    // Store name
    srv->functions[i]->name = malloc(strlen(name)+1);
    strcpy(srv->functions[i]->name, name);
    assert(srv->functions[i]->name);

    // Store handler
    srv->functions[i]->handler = handler;

    // Return function number
    return srv->num_func++;

}

int rpc_find_func(rpc_server *srv, char *name) {

    for (int i = 0; i < srv->num_func; i++) {
        if (strcmp(srv->functions[i]->name, name) == 0) {
            return i;
        }
    }

    return -1;

}

rpc_data *rpc_call_func(rpc_server *srv, int id, rpc_data *payload) {

    return srv->functions[id]->handler(payload);

}

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical 9 */
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) return;    

    socklen_t client_addr_size;
    int client_sock_fd = -1;
    struct sockaddr_in6 client_addr;
    char buffer[256];

    int queue_len = 10;
    if (listen(srv->sock_fd, queue_len) < 0) {
        perror("listen");
        // close(srv->sock_fd);
        return;
    }

    client_addr_size = sizeof(client_addr);

    client_sock_fd =
        accept(srv->sock_fd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock_fd < 0) {
        perror("accept");
        // close(srv->sock_fd);
        return;
    }

    while(1) {
         
		if (recv(client_sock_fd, buffer, 256, 0) < 0) {
			// perror("recv()");
			close(client_sock_fd);
			continue;
		}
        puts(buffer);

        char *header = strtok(buffer, ",");

        // Deal with FIND
        if (strncmp("FIND", header, 4) == 0) {

            char *name = strtok(NULL, ",");

            int id = rpc_find_func(srv, name);
            char ret[10];
            sprintf(ret, "%d", id);

            // Respond with function id
            if (send(client_sock_fd, ret, strlen(ret), 0) < 0) {
                perror("send()");
                close(client_sock_fd);
                continue;
            }

        // Deal with CALL
        } else if (strncmp("CALL", header, 4) == 0) {
            
            int id = atoi(strtok(NULL, ","));

            rpc_data *payload = rpc_str_to_data(buffer);

            rpc_data *res = rpc_call_func(srv, id, payload);
            char *message = rpc_data_to_str(res);
            // printf("%s", message);
            // puts("*********************");
            // puts(message);

            // Respond with rpc_data result as a string
            if (send(client_sock_fd, message, strlen(message), 0) < 0) {
                perror("send");
                close(client_sock_fd);
                continue;
            }

        } else {
            fprintf(stderr, "Unknown header");
            close(client_sock_fd);
            continue;
        }
	
	}

    /* Do TCP teardown */
    if (close(client_sock_fd) < 0) {
        perror("close()");
        client_sock_fd = -1;
    }

    return;

}




















/****************************** CLIENT-SIDE API *******************************/

struct rpc_client {
    /* Add variable(s) for client state */
    // char *srv_addr;
    // int port;
    int sock_fd;
};

struct rpc_handle {
    /* Add variable(s) for handle */
    int id;
};

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
rpc_client *rpc_init_client(char *addr, int port) {

    if (addr == NULL) return NULL;

    // Initialise client side
    int sock_fd = -1;
    struct addrinfo hints, *servinfo, *rp;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    char service[5];
    sprintf(service,"%d",port);
    if (getaddrinfo(addr, service, &hints, &servinfo) < 0) {
		perror("getaddrinfo");
		exit(EXIT_FAILURE);
	}

    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
		sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock_fd == -1) continue;
		if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) != -1) break;
		close(sock_fd);
	}

    if (rp == NULL) {
		perror("Unable to connect");
		exit(EXIT_FAILURE);
	}

    freeaddrinfo(servinfo);

    // Initialise client structure
    rpc_client *cl = malloc(sizeof(rpc_client *));
    if (!cl) return NULL;

    // cl->srv_addr = addr;
    // cl->port = port;
    cl->sock_fd = sock_fd;

    return cl;

}

rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (cl == NULL || name == NULL) return NULL;

    char header[6] = "FIND,";
    char message[256];
    strcpy(message, header);
    strcat(message, name);

	if (send(cl->sock_fd, message, strlen(message), 0) < 0) {
		perror("send");
		close(cl->sock_fd);
		return NULL;
	}
    
    char ret[256];
	if (recv(cl->sock_fd, ret, 256, 0) < 0) {
		perror("recv()");
		close(cl->sock_fd);
		return NULL;
	}

    puts(ret);

    rpc_handle *h = malloc(sizeof(rpc_handle *));
    assert(h);
    h->id = atoi(ret);
    return h;

}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    if (cl == NULL || h == NULL || payload == NULL) return NULL;

    char header[6] = "CALL,";
    char message[256];
    strcpy(message, header);
    char *s = rpc_data_to_str(payload);
    strcat(message, s);

    if (send(cl->sock_fd, message, strlen(message), 0) < 0) {
		perror("send");
		close(cl->sock_fd);
		return NULL;
	}
    
    char ret[256];
	if (recv(cl->sock_fd, ret, 256, 0) < 0) {
		perror("recv()");
		close(cl->sock_fd);
		return NULL;
	}

    puts(ret);

    rpc_data *data = rpc_str_to_data(ret);

    return data;

}

void rpc_close_client(rpc_client *cl) {

    if (cl == NULL) return;

	if (close(cl->sock_fd) < 0) {
		perror("close()");
		return;
	}

    free(cl);

    return;

}