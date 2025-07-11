#ifndef NET_H
#define NET_H

#include <sys/epoll.h>
#include <openssl/ssl.h>
#include "core.h"

// Creates a server socket, binds it to a port, and puts it in listen mode.
int create_server_socket(int port);

// Starts the main event loop for a worker process.
void worker_loop(int server_fd, int https_server_fd, core_config_t *core_config, SSL_CTX *ssl_ctx);

// Forward declaration of the connection structure
typedef struct connection_t connection_t;

// Accept multiple connections in a batch
int accept_connections_batch(int epoll_fd, int server_fd, int is_https, 
                           struct epoll_event* events, int* event_count,
                           SSL_CTX* ssl_ctx, core_config_t* core_config);

#endif  // NET_H 