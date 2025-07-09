#include "log.h"
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdarg.h>

static log_level_t current_log_level = LOG_LEVEL_INFO;
static FILE *error_log_file = NULL;
static FILE *access_log_file = NULL;
static access_log_format_t access_log_format = ACCESS_LOG_FORMAT_COMBINED;
static log_config_t *global_log_config = NULL;

// Function to initialize the logger
void log_init(const char *filename, log_level_t level) {
    current_log_level = level;
    
    if (filename && strcmp(filename, "stderr") != 0) {
        error_log_file = fopen(filename, "a");
        if (!error_log_file) {
            fprintf(stderr, "Failed to open error log file: %s\n", filename);
            error_log_file = stderr;
        }
    } else {
        error_log_file = stderr;
    }
}

// Initialize access logging
void access_log_init(const char *access_log_file_path, access_log_format_t format) {
    access_log_format = format;
    
    if (access_log_file_path && strcmp(access_log_file_path, "off") != 0) {
        access_log_file = fopen(access_log_file_path, "a");
        if (!access_log_file) {
            log_message(LOG_LEVEL_ERROR, "Failed to open access log file");
            return;
        }
        
        // Set line buffering for access log
        setvbuf(access_log_file, NULL, _IOLBF, 0);
    }
}

// Centralized logging function
void log_message(log_level_t level, const char *message) {
    // Only log if the message level is at or above the current log level
    if (level > current_log_level) {
        return;
    }

    time_t now = time(NULL);
    char time_buf[sizeof("2024-01-01 12:00:00")];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    const char *level_str;
    const char *level_color;
    FILE *output = error_log_file ? error_log_file : stderr;

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

    // Use colors only if outputting to stderr (terminal)
    if (output == stderr) {
        fprintf(output, "[%s] %s[%s]\x1B[0m [%d] %s\n", time_buf, level_color,
                level_str, getpid(), message);
    } else {
        fprintf(output, "[%s] [%s] [%d] %s\n", time_buf, level_str, getpid(), message);
    }
    
    fflush(output);
}

// Create a new access log entry
access_log_entry_t *create_access_log_entry(void) {
    access_log_entry_t *entry = calloc(1, sizeof(access_log_entry_t));
    if (!entry) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate access log entry");
        return NULL;
    }
    
    // Set default values
    gettimeofday(&entry->request_time, NULL);
    entry->client_ip = strdup("-");
    entry->remote_user = strdup("-");
    entry->method = strdup("-");
    entry->uri = strdup("-");
    entry->protocol = strdup("-");
    entry->status_code = 0;
    entry->response_size = 0;
    entry->referer = strdup("-");
    entry->user_agent = strdup("-");
    entry->server_name = strdup("-");
    entry->server_port = 0;
    entry->request_duration_ms = 0.0;
    entry->upstream_status = 0;
    entry->upstream_addr = strdup("-");
    entry->upstream_response_time_ms = 0.0;
    
    return entry;
}

// Free an access log entry
void free_access_log_entry(access_log_entry_t *entry) {
    if (!entry) return;
    
    free(entry->client_ip);
    free(entry->remote_user);
    free(entry->method);
    free(entry->uri);
    free(entry->protocol);
    free(entry->referer);
    free(entry->user_agent);
    free(entry->server_name);
    free(entry->upstream_addr);
    free(entry);
}

// Get current timestamp string for logs
char *get_log_timestamp(void) {
    static char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%d/%b/%Y:%H:%M:%S %z", localtime(&now));
    return timestamp;
}

// Get current timestamp string in ISO format
char *get_iso_timestamp(void) {
    static char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    return timestamp;
}

// Format access log entry in Common Log Format
static void format_common_log(const access_log_entry_t *entry, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%s %s %s [%s] \"%s %s %s\" %d %ld\n",
             entry->client_ip ? entry->client_ip : "-",
             "-", // identd (always -)
             entry->remote_user ? entry->remote_user : "-",
             get_log_timestamp(),
             entry->method ? entry->method : "-",
             entry->uri ? entry->uri : "-",
             entry->protocol ? entry->protocol : "-",
             entry->status_code,
             entry->response_size);
}

// Format access log entry in Combined Log Format
static void format_combined_log(const access_log_entry_t *entry, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%s %s %s [%s] \"%s %s %s\" %d %ld \"%s\" \"%s\" %.3f\n",
             entry->client_ip ? entry->client_ip : "-",
             "-", // identd (always -)
             entry->remote_user ? entry->remote_user : "-",
             get_log_timestamp(),
             entry->method ? entry->method : "-",
             entry->uri ? entry->uri : "-",
             entry->protocol ? entry->protocol : "-",
             entry->status_code,
             entry->response_size,
             entry->referer ? entry->referer : "-",
             entry->user_agent ? entry->user_agent : "-",
             entry->request_duration_ms / 1000.0);
}

// Format access log entry in JSON format
static void format_json_log(const access_log_entry_t *entry, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, 
             "{\"timestamp\":\"%s\",\"client_ip\":\"%s\",\"method\":\"%s\",\"uri\":\"%s\","
             "\"protocol\":\"%s\",\"status_code\":%d,\"response_size\":%ld,\"referer\":\"%s\","
             "\"user_agent\":\"%s\",\"server_name\":\"%s\",\"server_port\":%d,"
             "\"request_duration_ms\":%.3f,\"upstream_status\":%d,\"upstream_addr\":\"%s\","
             "\"upstream_response_time_ms\":%.3f}\n",
             get_iso_timestamp(),
             entry->client_ip ? entry->client_ip : "-",
             entry->method ? entry->method : "-",
             entry->uri ? entry->uri : "-",
             entry->protocol ? entry->protocol : "-",
             entry->status_code,
             entry->response_size,
             entry->referer ? entry->referer : "-",
             entry->user_agent ? entry->user_agent : "-",
             entry->server_name ? entry->server_name : "-",
             entry->server_port,
             entry->request_duration_ms,
             entry->upstream_status,
             entry->upstream_addr ? entry->upstream_addr : "-",
             entry->upstream_response_time_ms);
}

// Log an access entry
void log_access_entry(const access_log_entry_t *entry) {
    if (!access_log_file || !entry) {
        return;
    }
    
    char buffer[4096];
    
    switch (access_log_format) {
        case ACCESS_LOG_FORMAT_COMMON:
            format_common_log(entry, buffer, sizeof(buffer));
            break;
        case ACCESS_LOG_FORMAT_COMBINED:
            format_combined_log(entry, buffer, sizeof(buffer));
            break;
        case ACCESS_LOG_FORMAT_JSON:
            format_json_log(entry, buffer, sizeof(buffer));
            break;
        default:
            format_combined_log(entry, buffer, sizeof(buffer));
            break;
    }
    
    fprintf(access_log_file, "%s", buffer);
    fflush(access_log_file);
    
    // Check if log rotation is needed
    if (global_log_config && global_log_config->access_log_file) {
        check_log_rotation(global_log_config->access_log_file, 
                          global_log_config->log_rotation_size_mb);
    }
}

// Log performance metrics
void log_performance_metrics(const char *operation, double duration_ms, 
                            const char *additional_info) {
    if (!global_log_config || !global_log_config->enable_performance_logging) {
        return;
    }
    
    char message[512];
    snprintf(message, sizeof(message), "PERF: %s took %.3fms - %s", 
             operation, duration_ms, additional_info ? additional_info : "");
    
    log_message(LOG_LEVEL_INFO, message);
}

// Log structured error with context
void log_structured_error(log_level_t level, const char *component, 
                         const char *operation, const char *error_message, 
                         const char *context) {
    char structured_message[1024];
    snprintf(structured_message, sizeof(structured_message), 
             "[%s:%s] %s%s%s", 
             component, operation, error_message,
             context ? " - " : "",
             context ? context : "");
    
    log_message(level, structured_message);
}

// Check if log file needs rotation
int check_log_rotation(const char *log_file, int max_size_mb) {
    if (!log_file || max_size_mb <= 0) {
        return 0;
    }
    
    struct stat st;
    if (stat(log_file, &st) != 0) {
        return 0;
    }
    
    long size_mb = st.st_size / (1024 * 1024);
    if (size_mb >= max_size_mb) {
        return rotate_log_file(log_file);
    }
    
    return 0;
}

// Perform log rotation
int rotate_log_file(const char *log_file) {
    char old_log_file[512];
    char timestamp[32];
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    snprintf(old_log_file, sizeof(old_log_file), "%s.%s", log_file, timestamp);
    
    if (rename(log_file, old_log_file) != 0) {
        log_message(LOG_LEVEL_ERROR, "Failed to rotate log file");
        return -1;
    }
    
    // Reopen the log file if it's the access log
    if (access_log_file && global_log_config && global_log_config->access_log_file &&
        strcmp(log_file, global_log_config->access_log_file) == 0) {
        fclose(access_log_file);
        access_log_file = fopen(log_file, "a");
        if (access_log_file) {
            setvbuf(access_log_file, NULL, _IOLBF, 0);
        }
    }
    
    char message[256];
    snprintf(message, sizeof(message), "Log file rotated: %s -> %s", log_file, old_log_file);
    log_message(LOG_LEVEL_INFO, message);
    
    return 0;
}

// Initialize logging from configuration
void init_logging_from_config(const log_config_t *config) {
    if (!config) return;
    
    global_log_config = malloc(sizeof(log_config_t));
    if (!global_log_config) {
        log_message(LOG_LEVEL_ERROR, "Failed to allocate log configuration");
        return;
    }
    
    memcpy(global_log_config, config, sizeof(log_config_t));
    
    // Duplicate strings to avoid dangling pointers
    if (config->error_log_file) {
        global_log_config->error_log_file = strdup(config->error_log_file);
    }
    if (config->access_log_file) {
        global_log_config->access_log_file = strdup(config->access_log_file);
    }
    
    // Initialize error logging
    if (config->error_log_file) {
        log_init(config->error_log_file, config->error_log_level);
    }
    
    // Initialize access logging
    if (config->access_log_file) {
        access_log_init(config->access_log_file, config->access_log_format);
    }
    
    log_message(LOG_LEVEL_INFO, "Logging system initialized from configuration");
}

// Cleanup logging resources
void cleanup_logging(void) {
    if (error_log_file && error_log_file != stderr) {
        fclose(error_log_file);
        error_log_file = NULL;
    }
    
    if (access_log_file) {
        fclose(access_log_file);
        access_log_file = NULL;
    }
    
    if (global_log_config) {
        free(global_log_config->error_log_file);
        free(global_log_config->access_log_file);
        free(global_log_config);
        global_log_config = NULL;
    }
} 