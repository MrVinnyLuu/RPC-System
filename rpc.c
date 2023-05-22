#define _POSIX_C_SOURCE 200112L
#define _DEFAULT_SOURCE
#define NONBLOCKING

#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <endian.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define INIT_FUNCS_SIZE 4
#define LISTEN_QUEUE_LEN 16
#define HEADER_LEN 5 // including terminating NULL byte
#define MAX_DATA2_LEN 100000
#define PORT_NUM_STR 6 // including terminating NULL byte
#define MAX_PORT 65535

/********************************* SHARED API *********************************/

/* Send (up to) a 64-bit integer */
/* Returns -1 on failure/error and 0 otherwise */
int rpc_send_64(int sock_fd, size_t i) {
    // Convert host to network byte order
    uint64_t bytes = htobe64(i);
    if (send(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
        perror("send");
        return -1;
    }
    return 0;
}

/* Receive (up to) a 64-bit integer */
/* Returns -1 on failure/error */
size_t rpc_recv_64(int sock_fd) {
    uint64_t bytes = -1;
    if (recv(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
		perror("recv");
		return -1;
	}
    // Convert to network byte order to host
    return be64toh(bytes);
}

/* Check if the data is valid */
/* Returns 1 (true) for vaild data and 0 (false) for invaild data */
int rpc_data_valid(rpc_data *data) {
    if (data == NULL || data->data2_len < 0
        || (data->data2_len == 0 && data->data2 != NULL)
        || (data->data2_len > 0 && data->data2 == NULL)) {
        return 0;
    } else if (data->data2_len > MAX_DATA2_LEN) {
        perror("Overlength error");
        return 0;
    } else {
        return 1;
    }
}

void rpc_data_free(rpc_data *data) {
    if (data == NULL) return;
    if (data->data2 != NULL) {
        free(data->data2);
    }
    free(data);
}

/* Send a rpc_data */
/* Returns -1 on failure/error and 0 otherwise */
int rpc_data_send(int sock_fd, rpc_data *data) {

    if (sock_fd < 0 || !rpc_data_valid(data)) return -1;

    // Send data1
    if (rpc_send_64(sock_fd, data->data1) < 0) {
        perror("rpc_send_64");
        return -1;
    }

    // Send data2_len
    if (rpc_send_64(sock_fd, data->data2_len) < 0) {
        perror("rpc_send_64");
        return -1;
    }

    // Send data2
    if (data->data2_len > 0) {
        if (send(sock_fd, data->data2, data->data2_len, 0) < 0) {
            perror("send");
            return -1;
        }
    }

    return 0;

}

/* Receive a rpc_data */
/* Returns NULL on failure/error */
rpc_data *rpc_data_recv(int sock_fd) {

    // Initialise result
    rpc_data *res = malloc(sizeof(*res));
    if (!res) {
        perror("malloc");
        return NULL;
    }
    res->data2 = NULL;

    // Receive data1
    if ((res->data1 = rpc_recv_64(sock_fd)) < 0) {
        perror("rpc_recv_64");
        rpc_data_free(res);
        return NULL;
    }

    // Receive data2_len
    if ((res->data2_len = rpc_recv_64(sock_fd)) < 0) {
        perror("rpc_recv_64");
        rpc_data_free(res);
        return NULL;
    }

    // Receive data2
    if (res->data2_len > 0) {

        res->data2 = malloc(res->data2_len);
        if (!res->data2) {
            perror("malloc");
            rpc_data_free(res);
            return NULL;
        }

        if (recv(sock_fd, res->data2, res->data2_len, 0) < 0) {
            perror("recv");
            rpc_data_free(res);
            return NULL;
        }

    }

    return res;

}


/****************************** SERVER-SIDE API *******************************/

typedef struct func {
    char *name;
    rpc_handler handler;
} func_t;

struct rpc_server {
    int sock_fd;
    int num_func, cur_size;
    func_t **functions;
};

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
rpc_server *rpc_init_server(int port) {

    // Check for a vaild & usable port number for TCP
    if (port < 0 || port > MAX_PORT) return NULL;

    // Initialise server side 3 tuple
    int sock_fd = -1;
    struct addrinfo hints, *servinfo;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6; // IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    hints.ai_flags = AI_PASSIVE;
    char service[PORT_NUM_STR];
    sprintf(service, "%d", port);
    if (getaddrinfo(NULL, service, &hints, &servinfo) < 0) {
		perror("getaddrinfo");
		return NULL;
	}

    sock_fd = socket(servinfo->ai_family, servinfo->ai_socktype,
                     servinfo->ai_protocol);
	if (sock_fd < 0) {
        perror("socket");
        return NULL;
    }

    // Allow port/interface reuse
    int re = 1;
    if (setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
        perror("setsockopt");
        return NULL;
    }

    if (bind(sock_fd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
        perror("bind");
        return NULL;
    }

    freeaddrinfo(servinfo);

    // Initialise server structure
    rpc_server *srv = malloc(sizeof(*srv));
    if (!srv) {
        perror("malloc");
        return NULL;
    }

    srv->sock_fd = sock_fd;

    srv->functions = malloc(INIT_FUNCS_SIZE * sizeof(*(srv->functions)));
    if (!srv->functions) {
        perror("malloc");
        return NULL;
    }

    srv->num_func = 0;
    srv->cur_size = INIT_FUNCS_SIZE;

    return srv;

}

void rpc_close_server(rpc_server *srv) {
    if (srv == NULL) return;
    if (srv->functions) {
        for (int i = 0; i < srv->num_func; i++) {
            if (srv->functions[i]->name) free(srv->functions[i]->name);
            if (srv->functions[i]) free(srv->functions[i]);
        }
        free(srv->functions);
    }
    free(srv);
    srv = NULL;
    return;
}

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {

    if (!(srv && name && handler && srv->functions)) return -1;

    // Ensure there's space in the array
    if (srv->num_func == srv->cur_size) {
        srv->cur_size *= 2;
        srv->functions = realloc(srv->functions,
                                 srv->cur_size * sizeof(*(srv->functions)));
        if (!srv->functions) {
            perror("realloc");
            return -1;
        }
    }

    // Check for duplicate named functions
    int i;
    for (i = 0; i < srv->num_func; i++) {
        if (!srv->functions[i] || !srv->functions[i]->name) continue;
        if (strcmp(srv->functions[i]->name, name) == 0) {
            break;
        }
    }

    srv->functions[i] = malloc(sizeof(*(srv->functions[i])));
    if (!srv->functions[i]) {
        perror("malloc");
        return -1;
    }

    // Store name
    srv->functions[i]->name = malloc(strlen(name)+1);
    if (!srv->functions[i]->name) {
        perror("malloc");
        return -1;
    }
    strcpy(srv->functions[i]->name, name);

    // Store handler
    srv->functions[i]->handler = handler;

    // Return function number (not used)
    return srv->num_func++;

}

int rpc_find_func(rpc_server *srv, char *name) {

    if (!name || !srv || !srv->functions) return -1;

    for (int i = 0; i < srv->num_func; i++) {
        if (!srv->functions[i] || !srv->functions[i]->name) continue;
        if (strcmp(srv->functions[i]->name, name) == 0) {
            return i;
        }
    }

    return -1;

}

rpc_data *rpc_call_func(rpc_server *srv, int id, rpc_data *payload) {

    if (!srv || !payload || id >= srv->num_func || !srv->functions
        || !srv->functions[id] || !srv->functions[id]->handler) return NULL;

    return srv->functions[id]->handler(payload);
    
}

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical 9 */
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) return;    

    socklen_t client_addr_size;
    int client_sock_fd = -1, n = -1;
    struct sockaddr_in6 client_addr;

    if (listen(srv->sock_fd, LISTEN_QUEUE_LEN) < 0) {
        perror("listen");
        close(srv->sock_fd);
        return;
    }

    client_addr_size = sizeof(client_addr);

    while(1) {

        // Accept new connection
        client_sock_fd = accept(srv->sock_fd, (struct sockaddr*)&client_addr,
                                &client_addr_size);
        if (client_sock_fd < 0) {
            perror("accept");
            continue;
        }

        // Receive the request
        char req[HEADER_LEN];
        if ((n = recv(client_sock_fd, req, HEADER_LEN, 0)) < 0) {
			perror("recv");
			close(client_sock_fd);
			continue;
		}

        if (n == 0) continue;

        // Create a child process to handle the request
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            close(client_sock_fd);
            continue;
        // Let the parent process continue
        } else if (pid > 0) { 
            continue;
        }

        // Deal with FIND
        if (strcmp("FIND", req) == 0) {

            // Receive function name string length
            size_t len = rpc_recv_64(client_sock_fd);
            if (len < 0) {
                perror("rpc_recv_64");
                rpc_send_64(client_sock_fd, -1);
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }

            // Receive function name
            char *name = malloc(len+1); // +1 for NULL byte
            if (!name) {
                perror("malloc");
                rpc_send_64(client_sock_fd, -1);
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }
            if ((n = recv(client_sock_fd, name, len, 0)) < 0) {
                perror("recv");
                rpc_send_64(client_sock_fd, -1);
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }
            name[len] = '\0';

            // Find the function id
            int id = rpc_find_func(srv, name); // -1 if not found
            free(name);

            // Respond with function id
            if (rpc_send_64(client_sock_fd, id) < 0) {
                perror("rpc_send_64");
                rpc_send_64(client_sock_fd, -1);
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }

        // Deal with CALL
        } else if (strcmp("CALL", req) == 0) {

            // Receive function id
            int id = rpc_recv_64(client_sock_fd);

            // Receive the payload
            rpc_data *payload = rpc_data_recv(client_sock_fd);
            
            // Call the function
            rpc_data *res = rpc_call_func(srv, id, payload);

            // Check validity/success
            char response[HEADER_LEN]; // status message
            // On failure 
            if (!res || !rpc_data_valid(res)
                || !payload || !rpc_data_valid(payload)) {
                strcpy(response, "NULL");
                rpc_data_free(res);
                res = NULL;
            // On success
            } else {
                strcpy(response, "DATA");
            }
            rpc_data_free(payload);
            
            // Respond with the status message
            if (send(client_sock_fd, response, HEADER_LEN, 0) < 0) {
                perror("send");
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }

            // Return the result if valid (not NULL)
            if (res && rpc_data_send(client_sock_fd, res) < 0) {
                perror("rpc_data_send");
                send(client_sock_fd, "NULL", HEADER_LEN, 0);
                close(client_sock_fd);
                rpc_close_server(srv);
                return;
            }

            if (res) rpc_data_free(res);

        } else {
            fprintf(stderr, "Unknown request: %s", req);
        }

        // Close connection
        if (close(client_sock_fd) < 0) {
            perror("close");
        }

        rpc_close_server(srv);
        return;

	}

    return;

}


/****************************** CLIENT-SIDE API *******************************/

struct rpc_client {
    char *addr;
    int port;
};

struct rpc_handle {
    int id;
};

rpc_client *rpc_init_client(char *addr, int port) {

    if (addr == NULL || port < 0 || port > MAX_PORT) return NULL;

    // Initialise client structure
    rpc_client *cl = malloc(sizeof(*cl));
    if (!cl) {
        perror("malloc");
        return NULL;
    }

    cl->addr = malloc(strlen(addr)+1);
    if (!cl->addr) {
        perror("malloc");
        return NULL;
    }
    strcpy(cl->addr, addr);

    cl->port = port;

    return cl;

}

/* (Re)initiate a connection to the server */
/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
int rpc_connect_client(rpc_client *cl) {

    if (cl == NULL) return -1;

    int sock_fd = -1;
    struct addrinfo hints, *servinfo, *rp;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    char service[PORT_NUM_STR];
    sprintf(service,"%d",cl->port);
    if (getaddrinfo(cl->addr, service, &hints, &servinfo) < 0) {
		perror("getaddrinfo");
		return -1;
	}

    for (rp = servinfo; rp != NULL; rp = rp->ai_next) {
		sock_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sock_fd == -1) continue;
		if (connect(sock_fd, rp->ai_addr, rp->ai_addrlen) != -1) break;
		close(sock_fd);
	}
    
    freeaddrinfo(servinfo);

    if (rp == NULL) {
		perror("Unable to connect");
		return -1;
	}

    return sock_fd;

}

rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (cl == NULL || name == NULL) return NULL;

    // Connect to the server
    int sock_fd = -1;
    if ((sock_fd = rpc_connect_client(cl)) < 0) {
        perror("rpc_connect_client");
        return NULL;
    }

    // Send FIND request
	if (send(sock_fd, "FIND", HEADER_LEN, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}

    // Send string length
    size_t len = strlen(name);
    if (rpc_send_64(sock_fd, len) < 0) {
        perror("rpc_send_64");
        close(sock_fd);
        return NULL;
    }

    // Send name
	if (send(sock_fd, name, len, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}

    // Receive function id
    int id = rpc_recv_64(sock_fd); // Error handled after close()

    // Close connection
    if (close(sock_fd) < 0) {
        perror("close");
        // Not really any reason to return NULL for this error
    }

    if (id == -1) return NULL;

    // Create and return handle
    rpc_handle *h = malloc(sizeof(*h));
    if (!h) {
        perror("malloc");
        return NULL;
    }
    h->id = id;

    return h;

}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    if (cl == NULL || h == NULL || payload == NULL) return NULL;

    // Check validity of payload
    if (!rpc_data_valid(payload)) return NULL;

    // Connect to the server
    int sock_fd = -1;
    if ((sock_fd = rpc_connect_client(cl)) < 0) {
        perror("rpc_connect_client");
        return NULL;
    }

    // Send CALL request
	if (send(sock_fd, "CALL", HEADER_LEN, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}

    // Send function id
    if (rpc_send_64(sock_fd, h->id) < 0) {
        perror("rpc_send_64");
        close(sock_fd);
        return NULL;
    }

    // Send the payload
    if (rpc_data_send(sock_fd, payload) < 0) {
        perror("rpc_data_send");
        close(sock_fd);
        return NULL;
    }

    // Receive status message
    char response[HEADER_LEN];
	if (recv(sock_fd, response, HEADER_LEN, 0) < 0) {
		perror("recv");
		close(sock_fd);
		return NULL;
	}
    response[HEADER_LEN] = '\0';

    // Return on error
    if (strcmp(response, "NULL") == 0) return NULL;
    
    // Receive and return data
    rpc_data *res = rpc_data_recv(sock_fd); // Error handled after close()

    // Close connection
    if (close(sock_fd) < 0) {
        perror("close");
        // Not really any reason to return NULL for this error
    }

    // Check validity of result
    if (!rpc_data_valid(res)) return NULL;

    return res;

}

void rpc_close_client(rpc_client *cl) {
    if (cl == NULL) return;
    if (cl->addr) free(cl->addr);
    free(cl);
    cl = NULL;
    return;
}