#ifndef NET_H
#define NET_H

#include <openssl/ssl.h>
#include "core.h"

// Creates a server socket, binds it to a port, and puts it in listen mode.
int create_server_socket(int port);

// Starts the main event loop for a worker process.
void worker_loop(int server_fd, int https_server_fd, core_config_t *core_config, SSL_CTX *ssl_ctx);

#endif  // NET_H 