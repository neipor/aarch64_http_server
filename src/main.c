#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_PORT 8080
#define DEFAULT_HTTPS_PORT 8443
#define DEFAULT_WEB_ROOT "./www"
#define BUFFER_SIZE 4096
#define MAX_EVENTS 64
#define DEFAULT_PAGE "/index.html"
#define NOT_FOUND_PAGE "/404.html"

// Log levels
typedef enum { LOG_LEVEL_INFO, LOG_LEVEL_ERROR, LOG_LEVEL_DEBUG } log_level_t;

// Configuration structure
struct server_config {
    int port;
    int https_port;
    char web_root[256];
    char cert_file[256];
    char key_file[256];
    int num_workers;
};

// Global config
struct server_config config;

pid_t worker_pids[128];
int num_workers_spawned = 0;

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

void signal_handler(int signum) {
    char msg[128];
    snprintf(msg, sizeof(msg), "Received signal %d. Shutting down workers.",
             signum);
    log_message(LOG_LEVEL_INFO, msg);
    for (int i = 0; i < num_workers_spawned; i++) {
        kill(worker_pids[i], SIGKILL);
    }
    // Let the main loop's wait() handle reaping
}

// In-place trim whitespace from start and end of a string
void trim_whitespace(char *str) {
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
        snprintf(msg, sizeof(msg),
                 "Config file '%s' not found, using defaults.", filename);
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

// Function to handle errors and exit
void error_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

// Helper function to set a socket to non-blocking mode
static int make_socket_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL");
        return -1;
    }

    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL");
        return -1;
    }
    return 0;
}

// Simple function to get MIME type from file extension
const char *get_mime_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) return "application/octet-stream";
    if (strcmp(dot, ".html") == 0) return "text/html";
    if (strcmp(dot, ".css") == 0) return "text/css";
    if (strcmp(dot, ".js") == 0) return "application/javascript";
    if (strcmp(dot, ".jpg") == 0) return "image/jpeg";
    if (strcmp(dot, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(dot, ".png") == 0) return "image/png";
    return "application/octet-stream";
}

void handle_https_request(SSL *ssl, const char *client_ip) {
    char buffer[BUFFER_SIZE] = {0};
    int client_socket = SSL_get_fd(ssl);

    // Perform SSL handshake
    if (SSL_accept(ssl) <= 0) {
        ERR_print_errors_fp(stderr);
        log_message(LOG_LEVEL_ERROR, "SSL handshake failed.");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        return;
    }

    ssize_t bytes_read = SSL_read(ssl, buffer, sizeof(buffer) - 1);

    if (bytes_read <= 0) {
        int ssl_error = SSL_get_error(ssl, bytes_read);
        if (ssl_error != SSL_ERROR_WANT_READ &&
            ssl_error != SSL_ERROR_WANT_WRITE) {
            log_message(LOG_LEVEL_ERROR, "SSL_read error");
            ERR_print_errors_fp(stderr);
        }
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        return;
    }

    char *method = strtok(buffer, " \t\r\n");
    char *req_path = strtok(NULL, " \t\r\n");

    if (!method || !req_path) {
        log_message(LOG_LEVEL_ERROR, "Malformed request, closing connection.");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        return;
    }

    char log_msg[BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg), "HTTPS Request from %s: %s %s",
             client_ip, method, req_path);
    log_message(LOG_LEVEL_INFO, log_msg);

    // Security: prevent directory traversal
    if (strstr(req_path, "..")) {
        // For simplicity, just close connection on malicious-looking paths
        snprintf(log_msg, sizeof(log_msg),
                 "Directory traversal attempt from %s blocked.", client_ip);
        log_message(LOG_LEVEL_ERROR, log_msg);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        return;
    }

    char file_path[BUFFER_SIZE];
    if (strcmp(req_path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 DEFAULT_PAGE);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 req_path);
    }

    struct stat file_stat;
    int file_fd = -1;
    int status_code = 200;

    if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        status_code = 404;
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 NOT_FOUND_PAGE);
        stat(file_path, &file_stat);  // Get stats for the 404 page
        snprintf(log_msg, sizeof(log_msg),
                 "File not found: %s. Responding with 404.", req_path);
        log_message(LOG_LEVEL_INFO, log_msg);
    }

    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        // If even the 404 page can't be opened, something is very wrong.
        log_message(LOG_LEVEL_ERROR,
                    "Could not open 404 page, closing connection.");
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(client_socket);
        return;
    }

    const char *mime_type = get_mime_type(file_path);
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Server: ANX-Static/0.5.0\r\n"
             "Connection: close\r\n\r\n",
             status_code, (status_code == 200) ? "OK" : "Not Found", mime_type,
             file_stat.st_size);

    SSL_write(ssl, header, strlen(header));

    // sendfile doesn't work with SSL sockets directly, so we need to read and
    // write.
    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_sent;
    while ((bytes_read = read(file_fd, file_buffer, sizeof(file_buffer))) > 0) {
        bytes_sent = SSL_write(ssl, file_buffer, bytes_read);
        if (bytes_sent <= 0) {
            int ssl_error = SSL_get_error(ssl, bytes_sent);
            if (ssl_error != SSL_ERROR_WANT_WRITE) {
                log_message(LOG_LEVEL_ERROR,
                            "SSL_write error during sendfile emulation.");
                ERR_print_errors_fp(stderr);
                break;
            }
        }
    }

    close(file_fd);
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(client_socket);
}

void handle_http_request(int client_socket, const char *client_ip) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

    if (bytes_read <= 0) {
        if (bytes_read == -1 && errno != EAGAIN) {
            log_message(LOG_LEVEL_ERROR, "read error on http socket");
        }
        close(client_socket);
        return;
    }

    char *method = strtok(buffer, " \t\r\n");
    char *req_path = strtok(NULL, " \t\r\n");

    if (!method || !req_path) {
        log_message(LOG_LEVEL_ERROR,
                    "Malformed http request, closing connection.");
        close(client_socket);
        return;
    }

    char log_msg[BUFFER_SIZE];
    snprintf(log_msg, sizeof(log_msg), "HTTP Request from %s: %s %s", client_ip,
             method, req_path);
    log_message(LOG_LEVEL_INFO, log_msg);

    // Security: prevent directory traversal
    if (strstr(req_path, "..")) {
        // For simplicity, just close connection on malicious-looking paths
        snprintf(log_msg, sizeof(log_msg),
                 "Directory traversal attempt from %s blocked.", client_ip);
        log_message(LOG_LEVEL_ERROR, log_msg);
        close(client_socket);
        return;
    }

    char file_path[BUFFER_SIZE];
    if (strcmp(req_path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 DEFAULT_PAGE);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 req_path);
    }

    struct stat file_stat;
    int file_fd = -1;
    int status_code = 200;

    if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        status_code = 404;
        snprintf(file_path, sizeof(file_path), "%s%s", config.web_root,
                 NOT_FOUND_PAGE);
        stat(file_path, &file_stat);  // Get stats for the 404 page
        snprintf(log_msg, sizeof(log_msg),
                 "File not found: %s. Responding with 404.", req_path);
        log_message(LOG_LEVEL_INFO, log_msg);
    }

    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        // If even the 404 page can't be opened, something is very wrong.
        log_message(LOG_LEVEL_ERROR,
                    "Could not open 404 page, closing connection.");
        close(client_socket);
        return;
    }

    const char *mime_type = get_mime_type(file_path);
    char header[BUFFER_SIZE];
    snprintf(header, sizeof(header),
             "HTTP/1.1 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "Server: ANX-Static/0.5.0\r\n"
             "Connection: close\r\n\r\n",
             status_code, (status_code == 200) ? "OK" : "Not Found", mime_type,
             file_stat.st_size);

    write(client_socket, header, strlen(header));
    sendfile(client_socket, file_fd, NULL, file_stat.st_size);
    close(file_fd);
    close(client_socket);
}

void worker_loop(int server_fd, int https_server_fd, SSL_CTX *ssl_ctx) {
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) error_and_exit("epoll_create1 (worker)");

    event.data.fd = server_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        error_and_exit("epoll_ctl (worker)");
    }

    // Add HTTPS server socket to epoll
    event.data.fd = https_server_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_server_fd, &event) == -1) {
        error_and_exit("epoll_ctl (worker)");
    }

    log_message(LOG_LEVEL_INFO, "Worker process started.");

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP)) {
                char log_msg[128];
                snprintf(log_msg, sizeof(log_msg), "epoll error on fd %d",
                         events[i].data.fd);
                log_message(LOG_LEVEL_ERROR, log_msg);
                close(events[i].data.fd);
                continue;
            }

            int current_server_fd = events[i].data.fd;
            int is_https = (current_server_fd == https_server_fd);

            while (1) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd =
                    accept(current_server_fd, (struct sockaddr *)&client_addr,
                           &client_len);
                if (client_fd == -1) {
                    if ((errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                        log_message(LOG_LEVEL_ERROR, "accept failed");
                    }
                    break;
                }

                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip,
                          INET_ADDRSTRLEN);

                if (is_https) {
                    SSL *ssl = SSL_new(ssl_ctx);
                    SSL_set_fd(ssl, client_fd);
                    handle_https_request(ssl, client_ip);
                } else {
                    handle_http_request(client_fd, client_ip);
                }
            }
        }
    }
}

// Function to create and configure a server socket
int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in server_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        error_and_exit("socket failed");
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        error_and_exit("setsockopt failed");
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
        0) {
        char msg[100];
        snprintf(msg, sizeof(msg), "bind to port %d failed", port);
        error_and_exit(msg);
    }

    if (listen(server_fd, 128) < 0) {
        error_and_exit("listen failed");
    }

    if (make_socket_non_blocking(server_fd) == -1) {
        error_and_exit("make_socket_non_blocking failed");
    }

    return server_fd;
}

int main(int argc, char *argv[]) {
    int server_fd, https_server_fd;
    SSL_CTX *ssl_ctx = NULL;

    const char *config_file = "server.conf";
    if (argc > 1) {
        config_file = argv[1];
    }
    parse_config(config_file);

    // --- OpenSSL Initialization ---
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!ssl_ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Load certificate and private key
    if (SSL_CTX_use_certificate_file(ssl_ctx, config.cert_file,
                                     SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, config.key_file,
                                    SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Create server sockets
    server_fd = create_server_socket(config.port);
    char msg[128];
    snprintf(msg, sizeof(msg), "HTTP server listening on port %d", config.port);
    log_message(LOG_LEVEL_INFO, msg);

    https_server_fd = create_server_socket(config.https_port);
    snprintf(msg, sizeof(msg), "HTTPS server listening on port %d",
             config.https_port);
    log_message(LOG_LEVEL_INFO, msg);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    snprintf(msg, sizeof(msg), "Master process starting %d workers...",
             config.num_workers);
    log_message(LOG_LEVEL_INFO, msg);
    for (int i = 0; i < config.num_workers; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            error_and_exit("fork failed");
        } else if (pid == 0) {
            // Child process
            worker_loop(server_fd, https_server_fd, ssl_ctx);
            exit(0);  // Should not be reached
        } else {
            // Parent process
            worker_pids[i] = pid;
            num_workers_spawned++;
        }
    }

    // Wait for all worker processes to exit
    for (int i = 0; i < config.num_workers; i++) {
        wait(NULL);
    }

    log_message(LOG_LEVEL_INFO,
                "All workers have shut down. Master process exiting.");

    close(server_fd);
    close(https_server_fd);
    if (ssl_ctx) SSL_CTX_free(ssl_ctx);

    return 0;
}