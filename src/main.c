#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "net.h"

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
            perror("fork failed");
            exit(EXIT_FAILURE);
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