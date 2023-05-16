#define _POSIX_C_SOURCE 200112L

#include "rpc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <endian.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define INIT_FUNCS_SIZE 10
#define LISTEN_QUEUE_LEN 10
#define DEBUG 0

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

/****************************** SERVER-SIDE API *******************************/

typedef struct func {
    char *name;
    rpc_handler handler;
} func_t;

struct rpc_server {
    /* Add variable(s) for server state */
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
	if (sock_fd < 0) {
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
    rpc_server *srv = malloc(sizeof(*srv));
    if (!srv) {
        perror("malloc");
        return NULL;
    }

    // srv->port = port;
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
    // if (srv->num_func == srv->cur_size) {
    //     srv->cur_size *= 2;
    //     srv->functions = realloc(srv->functions,
    //                              srv->cur_size * sizeof(*(srv->functions)));
    //     if (!srv->functions) {
    //         perror("realloc");
    //         return -1;
    //     }
    // }

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

    if (!name) return -1;

    if (DEBUG) printf("Finding func: %s\n", name);

    for (int i = 0; i < srv->num_func; i++) {
        if (strcmp(srv->functions[i]->name, name) == 0) {
            return i;
        }
    }

    return -1;

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

    client_sock_fd =
        accept(srv->sock_fd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (client_sock_fd < 0) {
        perror("accept");
        close(srv->sock_fd);
        return;
    }

    int n;

    while(1) {

        char *buffer = malloc(256);
        if (!buffer) {
            perror("malloc");
            continue;
        }

		if ((n = recv(client_sock_fd, buffer, 256, 0)) < 0) {
			perror("recv");
			close(client_sock_fd);
			continue;
		}

        if (n == 0) continue;

        if (DEBUG) {
            puts("Received");
            puts(buffer);
            puts("\n");
        }

        char *req = strtok(buffer, ",");

        // Deal with FIND
        if (req && strcmp("FIND", req) == 0) {

            int len = atoi(strtok(NULL, ",")); 
            char *name = strtok(NULL, ",");
            name[len] = '\0';

            // int len;
            // char *name;
            // sscanf(body, "%d,%s", &len, name);
            // name[len] = '\0';

            int id = rpc_find_func(srv, name); /////////////////////////////////////////////////////////////////////////
            uint32_t bytes = htonl(id);

            // Respond with function id
            if (send(client_sock_fd, &bytes, sizeof(uint32_t), 0) < 0) {
                perror("send");
                close(client_sock_fd);
                continue;
            }

            if (DEBUG) {
                puts("Sent");
                char s[5];
                sprintf(s, "%d", id);
                puts(s);
                puts("\n");
            }

        // Deal with CALL
        } else if (req && strcmp("CALL", req) == 0) {
            
            rpc_data *payload = malloc(sizeof(*payload));
            if (!payload) {
                perror("malloc");
                continue;
            }

            char *body = strtok(NULL, buffer);
            int id;
            sscanf(body, "%d,%d,%zu", &id, &payload->data1, &payload->data2_len);

            // int id = atoi(strtok(NULL, ",")); ///////////////////////////////
            // payload->data1 = atoi(strtok(NULL, ",")); ///////////////////////
            // payload->data2_len = strtod(strtok(NULL, ","), NULL); ///////////

            if (payload->data2_len > 0) {

                payload->data2 = malloc(payload->data2_len);
                if (!payload->data2) {
                    perror("malloc");
                    continue;
                }

                if (recv(client_sock_fd, 
                         payload->data2, payload->data2_len, 0) < 0) {

                    perror("recv");
                    close(client_sock_fd);
                    continue;

                }

                if (DEBUG) {
                    puts("Received");
                    puts(payload->data2);
                    puts("\n");
                }

            } else {
                payload->data2 = NULL;
            }

            rpc_data *res = rpc_call_func(srv, id, payload);

            char *message = malloc(256);
            if (!message) {
                perror("malloc");
                continue;
            }

            if (res != NULL) {
                sprintf(message,"%d,%zu",res->data1,res->data2_len); ///////////
                message[strlen(message)] = '\0';
            } else {
                message = "NULL";
            }

            if (send(client_sock_fd, message, strlen(message), 0) < 0) {
                perror("send");
                close(client_sock_fd);
                continue;
            }
            if (DEBUG) {
                puts("Sent");
                puts(message);
                puts("\n");
            }

            if (res && res->data2_len > 0) {
                if (send(client_sock_fd, res->data2, res->data2_len, 0) < 0) {
                    perror("send");
                    close(client_sock_fd);
                    continue;
                }
                if (DEBUG) {
                    puts("Sent");
                    puts(res->data2);
                    puts("\n");
                }
            }

        } else {
            fprintf(stderr, "Unknown request: %s", buffer);
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
    if (!cl) {
        perror("malloc");
        return NULL;
    }

    // cl->srv_addr = addr;
    // cl->port = port;
    cl->sock_fd = sock_fd;

    return cl;

}

rpc_handle *rpc_find(rpc_client *cl, char *name) {

    if (cl == NULL || name == NULL) return NULL;

    char message[256];
    sprintf(message, "FIND,%zu,%s", strlen(name), name);

    message[strlen(message)] = '\0';

	if (send(cl->sock_fd, message, strlen(message), 0) < 0) {
		perror("send");
		close(cl->sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Sent");
        puts(message);
        puts("\n");
    }

    uint32_t id; ////////////////////////////////////////////////////////////////////////////////////////////////////////////
	if (recv(cl->sock_fd, &id, sizeof(uint32_t), 0) < 0) {
		perror("recv");
		close(cl->sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Received");
        char s[5];
        sprintf(s, "%d", id);
        puts(s);
        puts("\n");
    }
    if (id == -1) return NULL;

    rpc_handle *h = malloc(sizeof(*h));
    if (!h) {
        perror("malloc");
        return NULL;
    }
    h->id = ntohl(id);
    return h;

}

rpc_data *rpc_call(rpc_client *cl, rpc_handle *h, rpc_data *payload) {

    if (cl == NULL || h == NULL || payload == NULL) return NULL;

    char *message = malloc(256);
    if (!message) {
        perror("malloc");
        return NULL;
    }
    sprintf(message, "CALL,%d,%d,%zu", 
            h->id, payload->data1, payload->data2_len);

    message[strlen(message)] = '\0';

    if (send(cl->sock_fd, message, strlen(message), 0) < 0) {
		perror("send");
		close(cl->sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Sent");
        puts(message);
        puts("\n");
    }

    if (payload->data2_len > 0) {
        if (send(cl->sock_fd, payload->data2, payload->data2_len, 0) < 0) {
            perror("send");
            close(cl->sock_fd);
            return NULL;
        }
        if (DEBUG) {
            puts("Sent");
            puts(payload->data2);
            puts("\n");
        }
    }

    char *ret = malloc(256);
    if (!ret) {
        perror("malloc");
        return NULL;
    }
	if (recv(cl->sock_fd, ret, 256, 0) < 0) {
		perror("recv()");
		close(cl->sock_fd);
		return NULL;
	}
    if (DEBUG) {
        puts("Received");
        puts(ret);
        puts("\n");
    }

    if (strcmp(ret, "NULL") == 0) return NULL;

    rpc_data *res = malloc(sizeof(*res));
    if (!res) {
        perror("malloc");
        return NULL;
    }

    sscanf(ret, "%d,%zu", &res->data1, &res->data2_len);

    // res->data1 = atoi(strtok(ret, ",")); ////////////////////////////////////
    // res->data2_len = strtod(strtok(NULL, ","), NULL); ///////////////////////

    if (res->data2_len > 0) {

        res->data2 = malloc(res->data2_len);
        if (!res->data2) {
            perror("malloc");
            return NULL;
        }

        if (recv(cl->sock_fd, res->data2, res->data2_len, 0) < 0) {
            perror("recv");
            close(cl->sock_fd);
            return NULL;
        }
        if (DEBUG) {
            puts("Received");
            puts(res->data2);
            puts("\n");
        }

    } else {
        res->data2 = NULL;
    }

    return res;

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