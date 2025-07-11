#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

// Enum for log levels
typedef enum {
  LOG_LEVEL_ERROR,
  LOG_LEVEL_WARNING,
  LOG_LEVEL_INFO,
  LOG_LEVEL_DEBUG
} log_level_t;

// Access log formats
typedef enum {
  ACCESS_LOG_FORMAT_COMMON,     // Common Log Format
  ACCESS_LOG_FORMAT_COMBINED,   // Combined Log Format
  ACCESS_LOG_FORMAT_JSON        // JSON Format
} access_log_format_t;

// Access log entry structure
typedef struct {
  char *client_ip;
  char *remote_user;
  struct timeval request_time;
  char *method;
  char *uri;
  char *protocol;
  int status_code;
  long response_size;
  char *referer;
  char *user_agent;
  char *server_name;
  int server_port;
  double request_duration_ms;
  int upstream_status;
  char *upstream_addr;
  double upstream_response_time_ms;
  // 添加缺失的字段
  char *request_uri;
  time_t timestamp;
} access_log_entry_t;

// Log configuration
typedef struct {
  char *error_log_file;
  char *access_log_file;
  log_level_t error_log_level;
  access_log_format_t access_log_format;
  int log_rotation_size_mb;
  int log_rotation_days;
  int enable_performance_logging;
} log_config_t;

// Function to initialize the logger
void log_init(const char *filename, log_level_t level);

// Initialize access logging
void access_log_init(const char *access_log_file, access_log_format_t format);

// Centralized logging function
void log_message(log_level_t level, const char *message);

// Log an access entry
void log_access_entry(const access_log_entry_t *entry);

// Create a new access log entry
access_log_entry_t *create_access_log_entry(void);

// Free an access log entry
void free_access_log_entry(access_log_entry_t *entry);

// Log performance metrics
void log_performance_metrics(const char *operation, double duration_ms, 
                            const char *additional_info);

// Log structured error with context
void log_structured_error(log_level_t level, const char *component, 
                         const char *operation, const char *error_message, 
                         const char *context);

// Check if log file needs rotation
int check_log_rotation(const char *log_file, int max_size_mb);

// Perform log rotation
int rotate_log_file(const char *log_file);

// Get current timestamp string for logs
char *get_log_timestamp(void);

// Get current timestamp string in ISO format
char *get_iso_timestamp(void);

// Initialize logging from configuration
void init_logging_from_config(const log_config_t *config);

// Cleanup logging resources
void cleanup_logging(void);

#endif  // LOG_H 