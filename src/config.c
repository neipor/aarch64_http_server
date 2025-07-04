#include "config.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Define the global config structure instance.
struct server_config config;

// In-place trim whitespace from start and end of a string
static void trim_whitespace(char *str) {
  char *start = str;
  while (isspace((unsigned char)*start)) {
    start++;
  }

  char *end = str + strlen(str) - 1;
  while (end > start && isspace((unsigned char)*end)) {
    end--;
  }

  // Write new null terminator
  *(end + 1) = '\0';

  // Shift the string to the beginning
  if (start != str) {
    memmove(str, start, strlen(start) + 1);
  }
}

void parse_config(const char *filename) {
  // Set defaults first
  config.port = DEFAULT_PORT;
  config.https_port = DEFAULT_HTTPS_PORT;
  strncpy(config.web_root, DEFAULT_WEB_ROOT, sizeof(config.web_root) - 1);
  config.num_workers = 2;  // Default to 2 workers
  strncpy(config.cert_file, "certs/server.crt", sizeof(config.cert_file) - 1);
  strncpy(config.key_file, "certs/server.key", sizeof(config.key_file) - 1);

  FILE *file = fopen(filename, "r");
  if (!file) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Config file '%s' not found, using defaults.",
             filename);
    log_message(LOG_LEVEL_INFO, msg);
    return;
  }

  char line[512];
  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '#' || line[0] == '\n') continue;

    char *key = strtok(line, "=");
    char *value = strtok(NULL, "\n");

    if (key && value) {
      trim_whitespace(key);
      trim_whitespace(value);

      if (strcmp(key, "port") == 0) {
        config.port = atoi(value);
      } else if (strcmp(key, "https_port") == 0) {
        config.https_port = atoi(value);
      } else if (strcmp(key, "web_root") == 0) {
        strncpy(config.web_root, value, sizeof(config.web_root) - 1);
      } else if (strcmp(key, "cert_file") == 0) {
        strncpy(config.cert_file, value, sizeof(config.cert_file) - 1);
      } else if (strcmp(key, "key_file") == 0) {
        strncpy(config.key_file, value, sizeof(config.key_file) - 1);
      } else if (strcmp(key, "num_workers") == 0) {
        config.num_workers = atoi(value);
      }
    }
  }
  fclose(file);
} 