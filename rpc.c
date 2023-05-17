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
#include <pthread.h>

#define INIT_FUNCS_SIZE 4
#define LISTEN_QUEUE_LEN 16
#define HEADER_LEN 5
#define DEBUG 0

/********************************* SHARED API *********************************/

/* Returns 1 (true) for vaild data and 0 (false) for invaild data */
int rpc_data_valid(rpc_data *data) {
    if (data == NULL || data->data2_len < 0
        || (data->data2_len == 0 && data->data2 != NULL)
        || (data->data2_len > 0 && data->data2 == NULL)) {
        return 0;
    } else {
        return 1;
    }
}

/* Returns -1 on failure/error and 0 otherwise */
int rpc_data_send(int sock_fd, rpc_data *data) {

    // Send data1
    uint64_t bytes = htobe64(data->data1);
    if (send(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
        perror("send");
        close(sock_fd);
        return -1;
    }
    if (DEBUG) {
        puts("Sent data1");
        char s[100];
        sprintf(s, "%d as %ld", data->data1, bytes);
        puts(s);
        puts("\n");
    }

    // Send data2_len
    bytes = htobe64(data->data2_len);
    if (send(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
        perror("send");
        close(sock_fd);
        return -1;
    }
    if (DEBUG) {
        puts("Sent data2_len");
        char s[100];
        sprintf(s, "%ld as %ld", data->data2_len, bytes);
        puts(s);
        puts("\n");
    }

    // Send data2
    if (data->data2_len > 0) {
        if (send(sock_fd, data->data2, data->data2_len, 0) < 0) {
            perror("send");
            close(sock_fd);
            return -1;
        }
        if (DEBUG) {
            puts("Sent data2");
            puts(data->data2); /////////////////////////////////////////////////////////////////////////////////////////
            puts("\n");
        }
    }

    return 0;

}

/* Returns NULL on failure/error */
rpc_data *rpc_data_recv(int sock_fd) {

    // Initialise result
    rpc_data *res = malloc(sizeof(*res));
    if (!res) {
        perror("malloc");
        return NULL;
    }

    // Receive data1
    uint64_t bytes;
    if (recv(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
		perror("recv");
		close(sock_fd);
		return NULL;
	}
    res->data1 = be64toh(bytes);
    if (DEBUG) {
        puts("Received data1");
        char s[100];
        sprintf(s, "%d as %ld", res->data1, bytes);
        puts(s);
        puts("\n");
    }

    // Receive data2_len
    if (recv(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
		perror("recv");
		close(sock_fd);
		return NULL;
	}
    res->data2_len = be64toh(bytes);
    if (DEBUG) {
        puts("Received data2_len");
        char s[100];
        sprintf(s, "%ld as %ld", res->data2_len, bytes);
        puts(s);
        puts("\n");
    }

    // Receive data2
    if (res->data2_len > 0) {

        res->data2 = malloc(res->data2_len);
        if (!res->data2) {
            perror("malloc");
            return NULL;
        }

        if (recv(sock_fd, res->data2, res->data2_len, 0) < 0) {
            perror("recv");
            close(sock_fd);
            return NULL;
        }
        if (DEBUG) {
            puts("Received data2");
            puts(res->data2); //////////////////////////////////////////////////////////////////////////////////////////
            puts("\n");
        }

    } else {
        res->data2 = NULL;
    }

    return res;

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
    int sock_fd;
    int num_func, cur_size;
    func_t **functions;
};

typedef struct args {
    rpc_handler handler;
    rpc_data *data;
} args_t;

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
rpc_server *rpc_init_server(int port) {

    // Check for a vaild & usable port number for TCP
    // Source: https://en.wikipedia.org/wiki/Port_(computer_networking)
    if (port < 1 || port > 65535) return NULL;

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

int rpc_register(rpc_server *srv, char *name, rpc_handler handler) {

    if (!(srv && name && handler)) return -1;

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

    if (DEBUG) printf("Registered func: %s at %d\n", name, i);

    // Return function number
    return srv->num_func++;

}

int rpc_find_func(rpc_server *srv, char *name) {

    if (!name || !srv) return -1;

    if (DEBUG) printf("Finding func: %s\n", name);

    for (int i = 0; i < srv->num_func; i++) {
        if (strcmp(srv->functions[i]->name, name) == 0) {
            return i;
        }
    }

    return -1;

}

// void *rpc_call_func(void *args) {

//     // "Unpack" args
//     rpc_server *srv = ((args_t *)args)->srv;
//     int id = ((args_t *)args)->id;
//     rpc_data *payload = ((args_t *)args)->data;

//     if (DEBUG) printf("Calling func at %d\n", id);

//     if (!srv || !payload || id >= srv->num_func) return NULL;
    
//     // Call the function
//     rpc_data *res = srv->functions[id]->handler(payload)

//     // Put the result back into args
//     ((args_t *)args)->data = res;
    
//     return NULL;
    
// }

void *rpc_handler_wrapper(void *args) {

    rpc_handler handler = ((args_t *)args)->handler;
    rpc_data *payload = ((args_t *)args)->data;

    rpc_data *res = handler(payload);
    ((args_t *)args)->data = res;

    return NULL;

}

void *hello(void *input) {
    printf("**************hello world");
    return NULL;
}

rpc_data *rpc_call_func(rpc_server *srv, int id, rpc_data *payload) {

    if (DEBUG) printf("Calling func at %d\n", id);

    if (!srv || !payload || id >= srv->num_func) return NULL;
    
    return srv->functions[id]->handler(payload);
    
}

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical 9 */
void rpc_serve_all(rpc_server *srv) {

    if (srv == NULL) return;    

    socklen_t client_addr_size;
    int client_sock_fd = -1;
    struct sockaddr_in6 client_addr;

    if (listen(srv->sock_fd, LISTEN_QUEUE_LEN) < 0) {
        perror("listen");
        close(srv->sock_fd);
        return;
    }

    client_addr_size = sizeof(client_addr);

    int n;

    while(1) {

        // Accept new connection
        client_sock_fd = accept(srv->sock_fd, (struct sockaddr*)&client_addr,
                                &client_addr_size);
        if (client_sock_fd < 0) {
            perror("accept");
            continue;
        }

        char req[HEADER_LEN];

        if ((n = recv(client_sock_fd, req, HEADER_LEN, 0)) < 0) {
			perror("recv");
			close(client_sock_fd);
			continue;
		}

        if (n == 0) continue;

        if (DEBUG) {
            puts("Received");
            puts(req);
            puts("\n");
        }

        // Deal with FIND
        if (strcmp("FIND", req) == 0) {

            // Receive function name string length
            uint64_t bytes; 
            if (recv(client_sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
                perror("recv");
                close(client_sock_fd);
                continue;
            }
            size_t len = be64toh(bytes);
            if (DEBUG) {
                puts("Received");
                char s[100];
                sprintf(s, "%ld as %ld", len, bytes);
                puts(s);
                puts("\n");
            }

            // Receive function name
            char *name = malloc(len+1); // +1 for NULL byte
            if (!name) {
                perror("malloc");
                continue;
            }
            if ((n = recv(client_sock_fd, name, len, 0)) < 0) {
                perror("recv");
                close(client_sock_fd);
                continue;
            }
            name[len] = '\0';
            if (DEBUG) {
                puts("Received");
                puts(name);
                puts("\n");
            }

            // Find the function id
            int id = rpc_find_func(srv, name); // -1 if not found
            free(name);
            bytes = htobe64(id);

            // Respond with function id
            if (send(client_sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
                perror("send");
                close(client_sock_fd);
                continue;
            }
            if (DEBUG) {
                puts("Sent");
                char s[100];
                sprintf(s, "%d as %ld", id, bytes);
                puts(s);
                puts("\n");
            }

        // Deal with CALL
        } else if (strcmp("CALL", req) == 0) {

            // Receive function id
            uint64_t bytes;
            if (recv(client_sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
                perror("recv");
                close(client_sock_fd);
                continue;
            }
            int id = be64toh(bytes);
            if (DEBUG) {
                puts("Received");
                char s[100];
                sprintf(s, "%d as %ld", id, bytes);
                puts(s);
                puts("\n");
            }

            // Receive the payload
            rpc_data *payload = rpc_data_recv(client_sock_fd);

            // Create a new thread and call the function
            pthread_t thread_id;
            // args_t args = {.srv = srv, .id = id, .data = payload};
            args_t *args = malloc(sizeof(*args));
            args->handler = srv->functions[id]->handler;
            args->data = malloc(sizeof(payload));
            args->data = payload;

            pthread_create(&thread_id, NULL, rpc_handler_wrapper, (void *)args);
            pthread_join(thread_id, NULL);
            rpc_data *res = args->data;
            
            // rpc_data *res = srv->functions[id]->handler(payload);

            // Check validity/success
            char response[HEADER_LEN]; // status message
            if (!res || !rpc_data_valid(res)
                || !payload || !rpc_data_valid(payload)) {
                strcpy(response, "NULL");
                rpc_data_free(res);
                res = NULL;
            } else {
                strcpy(response, "DATA");
            }
            rpc_data_free(payload);
            
            // Respond with the status message
            if (send(client_sock_fd, response, HEADER_LEN, 0) < 0) {
                perror("send");
                close(client_sock_fd);
                continue;
            }
            if (DEBUG) {
                puts("Sent");
                puts(response);
                puts("\n");
            }

            // Return the result if valid (not NULL)
            if (res && rpc_data_send(client_sock_fd, res) < 0) {
                perror("rpc_data_send");
                close(client_sock_fd);
                continue;
            }

            if (res) rpc_data_free(res);

        } else {
            fprintf(stderr, "Unknown request: %s", req);
        }

         // Close connection
        if (close(client_sock_fd) < 0) {
            perror("close");
        }
	
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

/* Adapted from https://gist.github.com/jirihnidek/388271b57003c043d322 and
   Practical Week 9 */
rpc_client *rpc_init_client(char *addr, int port) {

    if (addr == NULL || port < 1 || port > 65535) return NULL;

    // Initialise client structure
    rpc_client *cl = malloc(sizeof(rpc_client *));
    if (!cl) {
        perror("malloc");
        return NULL;
    }

    cl->addr = malloc(sizeof(char) * strlen(addr));
    strcpy(cl->addr, addr);
    cl->port = port;

    return cl;

}

int rpc_connect(rpc_client *cl) {

    if (cl == NULL) return -1;

    int sock_fd = -1;
    struct addrinfo hints, *servinfo, *rp;

    memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET6; // IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP
    char service[5];
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
    if ((sock_fd = rpc_connect(cl)) < 0) {
        perror("rpc_connect");
        return NULL;
    }

    // Send FIND request
    char req[HEADER_LEN] = "FIND";
	if (send(sock_fd, req, HEADER_LEN, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Sent");
        puts(req);
        puts("\n");
    }

    // Send string length
    size_t len = strlen(name);
    uint64_t bytes = htobe64(len);
    if (send(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
        perror("send");
        close(sock_fd);
        return NULL;
    }
    if (DEBUG) {
        puts("Sent");
        char s[100];
        sprintf(s, "%ld as %ld", len, bytes);
        puts(s);
        puts("\n");
    }

    // Send name
	if (send(sock_fd, name, len, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Sent");
        puts(name);
        puts("\n");
    }

    // Receive function id
    if (recv(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
		perror("recv");
		close(sock_fd);
		return NULL;
	}
    int id = be64toh(bytes);
    if (DEBUG) {
        puts("Received");
        char s[100];
        sprintf(s, "%d as %ld", id, bytes);
        puts(s);
        puts("\n");
    }

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
    if ((sock_fd = rpc_connect(cl)) < 0) {
        perror("rpc_connect");
        return NULL;
    }

    // Send CALL request
    char req[HEADER_LEN] = "CALL";
	if (send(sock_fd, req, HEADER_LEN, 0) < 0) {
		perror("send");
		close(sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Sent");
        puts(req);
        puts("\n");
    }

    // Send function id
    uint64_t bytes = htobe64(h->id);
    if (send(sock_fd, &bytes, sizeof(uint64_t), 0) < 0) {
        perror("send");
        close(sock_fd);
        return NULL;
    }
    if (DEBUG) {
        puts("Sent");
        char s[100];
        sprintf(s, "%d as %ld", h->id, bytes);
        puts(s);
        puts("\n");
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
    if (DEBUG) {
        puts("Received");
        puts(response);
        puts("\n");
    }

    // Return on error
    if (strcmp(response, "NULL") == 0) return NULL;
    
    // Receive and return data
    rpc_data *res = rpc_data_recv(sock_fd);

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

	// if (close(cl->sock_fd) < 0) {
	// 	perror("close");
	// 	return;
	// }

    free(cl);
    cl = NULL;

    return;

}