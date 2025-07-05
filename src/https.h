#ifndef HTTPS_H
#define HTTPS_H

#include <openssl/ssl.h>

void handle_https_request(SSL *ssl, const char *client_ip);

#endif  // HTTPS_H 