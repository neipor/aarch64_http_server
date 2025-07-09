#ifndef HTTPS_H
#define HTTPS_H

#include <openssl/ssl.h>
#include "core.h"

void handle_https_request(SSL *ssl, const char *client_ip, core_config_t *core_conf);

#endif  // HTTPS_H 