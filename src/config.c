#include "config.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global variable to store the directory of the config file
static char config_dir[256] = {0};

// Helper function to resolve relative paths
char* resolve_config_path(const char* path) {
    if (path == NULL) return NULL;
    
    // If path is already absolute, return a copy
    if (path[0] == '/') {
        char* result = malloc(strlen(path) + 1);
        strcpy(result, path);
        return result;
    }
    
    // If config_dir is not set, use current directory
    if (config_dir[0] == '\0') {
        char* result = malloc(strlen(path) + 1);
        strcpy(result, path);
        return result;
    }
    
    // Combine config_dir with relative path
    char* result = malloc(strlen(config_dir) + strlen(path) + 2);
    sprintf(result, "%s/%s", config_dir, path);
    return result;
}

const char *get_directive_value(const char *key, const directive_t *directives,
                                int count) {
  if (!directives) return NULL;
  for (int i = 0; i < count; i++) {
    if (directives[i].key && strcmp(directives[i].key, key) == 0) {
      return directives[i].value;
    }
  }
  return NULL;
}

// --- Private Helper Functions ---

// Simple tokenizer state machine
typedef enum {
  TOKEN_STATE_START,
  TOKEN_STATE_WORD,
  TOKEN_STATE_COMMENT
} token_state_t;

// For now, we'll use a simple static array for tokens.
// A more robust implementation would use a dynamic array.
#define MAX_TOKENS 2048
static char *tokens[MAX_TOKENS];
static int token_count = 0;

static void free_tokens() {
    for (int i = 0; i < token_count; i++) {
        free(tokens[i]);
        tokens[i] = NULL;
    }
    token_count = 0;
}

static void tokenize(char *content) {
  free_tokens();
  token_state_t state = TOKEN_STATE_START;
  char *start = content;

  for (char *p = content; *p != '\0'; p++) {
    if (token_count >= MAX_TOKENS) {
        log_message(LOG_LEVEL_ERROR, "Exceeded maximum number of tokens.");
        return;
    }
    switch (state) {
      case TOKEN_STATE_START:
        if (*p == '#') {
          state = TOKEN_STATE_COMMENT;
        } else if (isspace((unsigned char)*p)) {
          // just skip
        } else if (*p == '{' || *p == '}' || *p == ';') {
          tokens[token_count++] = strndup(p, 1);
        } else {
          start = p;
          state = TOKEN_STATE_WORD;
        }
        break;
      case TOKEN_STATE_WORD:
        if (isspace((unsigned char)*p) || *p == '{' || *p == '}' || *p == ';') {
          tokens[token_count++] = strndup(start, p - start);
          state = TOKEN_STATE_START;
          p--;  // re-evaluate this character
        }
        break;
      case TOKEN_STATE_COMMENT:
        if (*p == '\n') {
          state = TOKEN_STATE_START;
        }
        break;
    }
  }
}

// --- Recursive Parsing Logic ---
// Forward declarations
static http_block_t *parse_http_block(int *token_idx);
static server_block_t *parse_server_block(int *token_idx);
static location_block_t *parse_location_block(int *token_idx);
static directive_t parse_directive(int *token_idx);

static directive_t parse_directive(int *token_idx) {
  directive_t dir = {0};

  if (*token_idx + 1 >= token_count) {
    log_message(LOG_LEVEL_ERROR, "Unexpected end of config, expected a directive.");
    return dir;
  }

  dir.key = strdup(tokens[*token_idx]);
  (*token_idx)++;

  // Combine all tokens until a semicolon into the value
  char value_buffer[1024] = {0};
  int value_len = 0;
  while (*token_idx < token_count && strcmp(tokens[*token_idx], ";") != 0) {
    if (value_len > 0) {
      strncat(value_buffer, " ", sizeof(value_buffer) - value_len -1);
      value_len++;
    }
    strncat(value_buffer, tokens[*token_idx], sizeof(value_buffer) - value_len - 1);
    value_len += strlen(tokens[*token_idx]);
    (*token_idx)++;
  }

  if (strlen(value_buffer) > 0) {
    dir.value = strdup(value_buffer);
  }

  if (*token_idx < token_count && strcmp(tokens[*token_idx], ";") == 0) {
    (*token_idx)++;  // Consume semicolon
  } else {
    log_message(LOG_LEVEL_WARNING, "Directive not terminated with ';'.");
  }

  return dir;
}

static location_block_t *parse_location_block(int *token_idx) {
  if (strcmp(tokens[*token_idx], "location") != 0) {
    log_message(LOG_LEVEL_ERROR, "Expected 'location' block.");
    return NULL;
  }
  *token_idx += 1; // Consume 'location'

  location_block_t *loc = calloc(1, sizeof(location_block_t));
  loc->path = strdup(tokens[*token_idx]);
  *token_idx += 1; // Consume path

  if (strcmp(tokens[*token_idx], "{") != 0) {
    log_message(LOG_LEVEL_ERROR, "Expected '{' after location path.");
    free(loc->path);
    free(loc);
    return NULL;
  }
  *token_idx += 1; // Consume '{'
  
  loc->directives = NULL;
  loc->directive_count = 0;

  while (*token_idx < token_count && strcmp(tokens[*token_idx], "}") != 0) {
      loc->directive_count++;
      loc->directives =
          realloc(loc->directives, sizeof(directive_t) * loc->directive_count);
      loc->directives[loc->directive_count - 1] = parse_directive(token_idx);
  }
  
  if (strcmp(tokens[*token_idx], "}") != 0) {
      log_message(LOG_LEVEL_ERROR, "Location block not closed with '}'.");
  }
  *token_idx += 1;  // Consume '}'
  return loc;
}

static server_block_t *parse_server_block(int *token_idx) {
  if (strcmp(tokens[*token_idx], "server") != 0 ||
      strcmp(tokens[*token_idx + 1], "{") != 0) {
    log_message(LOG_LEVEL_ERROR, "Expected 'server {' block.");
    return NULL;
  }
  *token_idx += 2;  // Consume 'server' and '{'

  server_block_t *srv = calloc(1, sizeof(server_block_t));
  srv->directives = NULL;
  srv->directive_count = 0;
  srv->locations = NULL;

  while (*token_idx < token_count && strcmp(tokens[*token_idx], "}") != 0) {
    if (strcmp(tokens[*token_idx], "location") == 0) {
      location_block_t* loc = parse_location_block(token_idx);
      loc->next = srv->locations;
      srv->locations = loc;
    } else {
      // It's a directive
      srv->directive_count++;
      srv->directives =
          realloc(srv->directives, sizeof(directive_t) * srv->directive_count);
      srv->directives[srv->directive_count - 1] = parse_directive(token_idx);
    }
  }
  
  if (strcmp(tokens[*token_idx], "}") != 0) {
      log_message(LOG_LEVEL_ERROR, "Server block not closed with '}'.");
  }
  *token_idx += 1;  // Consume '}'
  return srv;
}

static http_block_t *parse_http_block(int *token_idx) {
  if (strcmp(tokens[*token_idx], "http") != 0 ||
      strcmp(tokens[*token_idx + 1], "{") != 0) {
    log_message(LOG_LEVEL_ERROR, "Expected 'http {' block.");
    return NULL;
  }
  *token_idx += 2;  // Consume 'http' and '{'

  http_block_t *http = calloc(1, sizeof(http_block_t));
  http->directives = NULL;
  http->directive_count = 0;
  http->servers = NULL;

  while (*token_idx < token_count && strcmp(tokens[*token_idx], "}") != 0) {
    if (strcmp(tokens[*token_idx], "server") == 0) {
      server_block_t* srv = parse_server_block(token_idx);
      srv->next = http->servers;
      http->servers = srv;
    } else {
      // It's a directive
      http->directive_count++;
      http->directives =
          realloc(http->directives, sizeof(directive_t) * http->directive_count);
      http->directives[http->directive_count - 1] = parse_directive(token_idx);
    }
  }

  if (strcmp(tokens[*token_idx], "}") != 0) {
      log_message(LOG_LEVEL_ERROR, "Http block not closed with '}'.");
  }
  *token_idx += 1;  // Consume '}'
  return http;
}

// --- Public API ---

config_t *parse_config(const char *filename) {
    // Extract directory from filename
    const char* last_slash = strrchr(filename, '/');
    if (last_slash) {
        size_t dir_len = last_slash - filename;
        strncpy(config_dir, filename, dir_len);
        config_dir[dir_len] = '\0';
    } else {
        // No directory in filename, use current directory
        config_dir[0] = '\0';
    }
    
    FILE *file = fopen(filename, "r");
    if (!file) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Config file '%s' not found.", filename);
        log_message(LOG_LEVEL_ERROR, msg);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(length + 1);
    if (!content) {
        log_message(LOG_LEVEL_ERROR, "Could not allocate memory for config file.");
        fclose(file);
        return NULL;
    }
    fread(content, 1, length, file);
    content[length] = '\0';
    fclose(file);

    tokenize(content);
    free(content);

    config_t *new_config = calloc(1, sizeof(config_t));
    int token_idx = 0;
    while (token_idx < token_count) {
        if (strcmp(tokens[token_idx], "http") == 0) {
            new_config->http = parse_http_block(&token_idx);
        } else {
            // Skip unknown top-level blocks for now
            log_message(LOG_LEVEL_DEBUG, "Skipping unknown top-level block");
            token_idx++;
        }
    }

    // Debug print the parsed directives
    if (new_config->http) {
        log_message(LOG_LEVEL_DEBUG, "--- Parsed Configuration ---");
        for (int i = 0; i < new_config->http->directive_count; i++) {
            printf("http > %s: %s\n", new_config->http->directives[i].key,
                   new_config->http->directives[i].value);
        }
        server_block_t *srv = new_config->http->servers;
        int server_num = 0;
        while (srv) {
            printf("http > server #%d:\n", server_num++);
            for (int i = 0; i < srv->directive_count; i++) {
                // Note: some values can be multipart, like 'listen 443 ssl'
                // Our simple key/value directive parser won't handle that correctly yet.
                if (srv->directives[i].key && srv->directives[i].value) {
                    printf("  %s: %s\n", srv->directives[i].key,
                           srv->directives[i].value);
                }
            }
            location_block_t *loc = srv->locations;
            while(loc) {
                printf("  location %s:\n", loc->path);
                for (int i = 0; i < loc->directive_count; i++) {
                    if(loc->directives[i].key && loc->directives[i].value) {
                        printf("    %s: %s\n", loc->directives[i].key, loc->directives[i].value);
                    }
                }
                loc = loc->next;
            }
            srv = srv->next;
        }
        printf("--- End Parsed Configuration ---\n");
    }

    // Cleanup tokens after use
    free_tokens();

    return new_config;
}

static void free_directive(directive_t *dir) {
    if (!dir) return;
    free(dir->key);
    dir->key = NULL;
    free(dir->value);
    dir->value = NULL;
}

static void free_location_block(location_block_t *loc) {
    if (!loc) return;
    free(loc->path);
    for (int i = 0; i < loc->directive_count; i++) {
        free_directive(&loc->directives[i]);
    }
    free(loc->directives);
    if(loc->next) {
        free_location_block(loc->next);
    }
    free(loc);
}

static void free_server_block(server_block_t *srv) {
    if (!srv) return;
    for (int i = 0; i < srv->directive_count; i++) {
        free_directive(&srv->directives[i]);
    }
    free(srv->directives);
    free_location_block(srv->locations);
    if(srv->next) {
        free_server_block(srv->next);
    }
    free(srv);
}

static void free_http_block(http_block_t *http) {
    if (!http) return;
    for (int i = 0; i < http->directive_count; i++) {
        free_directive(&http->directives[i]);
    }
    free(http->directives);
    free_server_block(http->servers);
    free(http);
}

void free_config(config_t *config) {
  if (!config) return;
  free_http_block(config->http);
  free(config);
} 