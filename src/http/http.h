#ifndef HTTP_H
#define HTTP_H

#include "core.h"

void handle_http_request(int client_socket, const char *client_ip, core_config_t *core_conf);

#endif  // HTTP_H 