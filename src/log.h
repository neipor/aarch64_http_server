#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>
#include <unistd.h>

// Log levels
typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_ERROR
} log_level_t;

// Centralized logging function
void log_message(log_level_t level, const char *message);

#endif  // LOG_H 