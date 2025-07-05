#include "log.h"

// Centralized logging function
void log_message(log_level_t level, const char *message) {
  time_t now = time(NULL);
  char time_buf[sizeof("2024-01-01 12:00:00")];
  strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

  const char *level_str;
  const char *level_color;

  switch(level) {
    case LOG_LEVEL_DEBUG:
        level_str = "DEBUG";
        level_color = "\x1B[36m"; // Cyan
        break;
    case LOG_LEVEL_INFO:
      level_str = "INFO";
      level_color = "\x1B[32m";  // Green
      break;
    case LOG_LEVEL_WARNING:
      level_str = "WARNING";
      level_color = "\x1B[33m"; // Yellow
      break;
    case LOG_LEVEL_ERROR:
      level_str = "ERROR";
      level_color = "\x1B[31m";  // Red
      break;
    default:
        level_str = "UNKNOWN";
        level_color = "\x1B[0m"; // Reset
        break;
  }

  fprintf(stdout, "[%s] %s[%s]\x1B[0m [%d] %s\n", time_buf, level_color,
          level_str, getpid(), message);
} 