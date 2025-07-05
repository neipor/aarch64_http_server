#ifndef CONFIG_H
#define CONFIG_H

#include <openssl/ssl.h>

// Represents a single "key value;" directive in nginx config
typedef struct {
  char *key;
  char *value;
} directive_t;

// Represents a location /path { ... } block
typedef struct location_block {
  char *path;
  directive_t *directives;
  int directive_count;
  struct location_block *next;
} location_block_t;

// Represents a server { ... } block
typedef struct server_block {
  directive_t *directives;
  int directive_count;
  location_block_t *locations;
  struct server_block *next;
} server_block_t;

// Represents the main http { ... } block
typedef struct {
  directive_t *directives;
  int directive_count;
  server_block_t *servers;
} http_block_t;

// The root of our entire configuration
typedef struct {
  http_block_t *http;
  // We can add other top-level blocks like 'events' here later
} config_t;

// Helper function to find the value of a directive within an array.
const char *get_directive_value(const char *key, const directive_t *directives, int count);

// Function to parse the configuration file and populate the global config struct.
config_t *parse_config(const char *filename);

// Function to free the memory allocated for the configuration
void free_config(config_t *config);

// Helper function to resolve relative paths based on config file location
char* resolve_config_path(const char* path);

#endif  // CONFIG_H 