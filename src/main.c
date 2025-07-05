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
    const char *config_file = "server.conf";
    if (argc > 1) {
        config_file = argv[1];
    }
    g_config = parse_config(config_file);
    if (!g_config) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse configuration. Exiting.");
        return EXIT_FAILURE;
    }

    // NOTE: The following logic is now broken as it depends on the old config structure.
    // This is a temporary state. We will fix this by creating a new `core` module
    // that uses the new config structure to get server settings.
    // For now, we will hardcode the ports to allow compilation.
    
    int http_port = 8080;
    int https_port = 8443;
    int num_workers = 2;
    const char* cert_file = "certs/server.crt";
    const char* key_file = "certs/server.key";


    // --- OpenSSL Initialization ---
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX *ssl_ctx = SSL_CTX_new(TLS_server_method());

    if (!ssl_ctx) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    // Create server sockets
    int server_fd = create_server_socket(http_port);
    char msg[128];
    snprintf(msg, sizeof(msg), "HTTP server listening on port %d", http_port);
    log_message(LOG_LEVEL_INFO, msg);

    int https_server_fd = create_server_socket(https_port);
    snprintf(msg, sizeof(msg), "HTTPS server listening on port %d", https_port);
    log_message(LOG_LEVEL_INFO, msg);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    snprintf(msg, sizeof(msg), "Master process starting %d workers...", num_workers);
    log_message(LOG_LEVEL_INFO, msg);
    for (int i = 0; i < num_workers; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            worker_loop(server_fd, https_server_fd, ssl_ctx);
            exit(0);
        } else {
            worker_pids[i] = pid;
            num_workers_spawned++;
        }
    }

    // Wait for all worker processes to exit
    for (int i = 0; i < num_workers; i++) {
        wait(NULL);
    }

    log_message(LOG_LEVEL_INFO, "All workers have shut down. Master process exiting.");
    
    close(server_fd);
    close(https_server_fd);
    if (ssl_ctx) SSL_CTX_free(ssl_ctx);
    free_config(g_config);

    return 0;
}