#include "log.h"

// Centralized logging function
void log_message(log_level_t level, const char *message) {
  time_t now = time(NULL);
  char buf[sizeof("2024-01-01 12:00:00")];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", gmtime(&now));

  const char *level_str = "INFO";
  if (level == LOG_LEVEL_ERROR) {
    level_str = "ERROR";
  } else if (level == LOG_LEVEL_DEBUG) {
    level_str = "DEBUG";
  }

  // Log to stdout for now. Could be redirected to a file.
  printf("[%s] [%s] [%d] %s\n", buf, level_str, getpid(), message);
  fflush(stdout);
} 