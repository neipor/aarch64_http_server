#ifndef CONFIG_H
#define CONFIG_H

#define DEFAULT_PORT 8080
#define DEFAULT_HTTPS_PORT 8443
#define DEFAULT_WEB_ROOT "./www"
#define DEFAULT_PAGE "/index.html"
#define NOT_FOUND_PAGE "/404.html"

// Configuration structure
struct server_config {
  int port;
  int https_port;
  char web_root[256];
  char cert_file[256];
  char key_file[256];
  int num_workers;
};

// The global server configuration, accessible by all modules.
// The actual instance is defined in config.c
extern struct server_config config;

// Function to parse the configuration file and populate the global config struct.
void parse_config(const char *filename);

#endif  // CONFIG_H 