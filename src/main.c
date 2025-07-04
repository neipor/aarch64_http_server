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

#include "config.h"
#include "http.h"
#include "https.h"
#include "log.h"
#include "util.h"

#define MAX_EVENTS 64

pid_t worker_pids[128];
int num_workers_spawned = 0;

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