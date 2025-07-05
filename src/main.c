#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "core.h"
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

    // 1. Parse config file into a syntax tree
    g_config = parse_config(config_file);
    if (!g_config) {
        log_message(LOG_LEVEL_ERROR, "Failed to parse configuration. Exiting.");
        return EXIT_FAILURE;
    }

    // 2. Process the syntax tree into a usable core configuration
    core_config_t *core_conf = create_core_config(g_config);
    if (!core_conf) {
        log_message(LOG_LEVEL_ERROR, "Failed to process configuration. Exiting.");
        free_config(g_config);
        return EXIT_FAILURE;
    }
    
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX *ssl_ctx = NULL;

    // Find the first SSL socket to initialize a global SSL_CTX
    // A better implementation would have per-server SSL_CTX.
    for (int i = 0; i < core_conf->listening_socket_count; i++) {
        if (core_conf->listening_sockets[i].is_ssl) {
            const char *cert_file = core_conf->listening_sockets[i].ssl_certificate;
            const char *key_file = core_conf->listening_sockets[i].ssl_certificate_key;

            if (!cert_file || !key_file) {
                log_message(LOG_LEVEL_WARNING,
                            "SSL socket configured but ssl_certificate or "
                            "ssl_certificate_key is missing.");
                continue;
            }
            
            // Use our real certs now
            if (strcmp(cert_file, "/etc/anx/certs/cert.pem")==0) {
                cert_file = "certs/server.crt";
                key_file = "certs/server.key";
            }

            ssl_ctx = SSL_CTX_new(TLS_server_method());
            if (!ssl_ctx) {
                ERR_print_errors_fp(stderr);
                exit(EXIT_FAILURE);
            }

            if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file,
                                             SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                log_message(LOG_LEVEL_ERROR, "Failed to load certificate file.");
                exit(EXIT_FAILURE);
            }
            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file,
                                            SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                log_message(LOG_LEVEL_ERROR, "Failed to load private key file.");
                exit(EXIT_FAILURE);
            }
            log_message(LOG_LEVEL_INFO, "SSL Context initialized successfully.");
            break; // Only init one global context for now
        }
    }

    // 3. Create listening sockets based on core config
    int http_fd = -1;
    int https_fd = -1;
    for(int i = 0; i < core_conf->listening_socket_count; i++) {
        int port = core_conf->listening_sockets[i].port;
        int is_ssl = core_conf->listening_sockets[i].is_ssl;
        
        int fd = create_server_socket(port);
        if (fd < 0) {
            // Mark this socket as invalid, but don't exit
            core_conf->listening_sockets[i].fd = -1;
            continue; 
        }
        core_conf->listening_sockets[i].fd = fd;

        char msg[128];
        snprintf(msg, sizeof(msg), "%s server listening on port %d", 
                is_ssl ? "HTTPS" : "HTTP", port);
        log_message(LOG_LEVEL_INFO, msg);
        
        // This is a simplification. We should pass a list of FDs to the worker.
        // For now, we just grab the first http and https fd we find.
        if (is_ssl) {
            if (https_fd == -1) https_fd = fd;
            if (!ssl_ctx) log_message(LOG_LEVEL_WARNING, "HTTPS socket open but no SSL_CTX initialized. HTTPS will not work.");
        }
        if (!is_ssl && http_fd == -1) http_fd = fd;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    char msg[128];
    snprintf(msg, sizeof(msg), "Master process starting %d workers...", core_conf->worker_processes);
    log_message(LOG_LEVEL_INFO, msg);
    log_message(LOG_LEVEL_DEBUG, "--> main: Forking workers...");
    
    for (int i = 0; i < core_conf->worker_processes; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork failed");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // Pass only the valid FDs
            int worker_http_fd = -1;
            int worker_https_fd = -1;
            for (int j = 0; j < core_conf->listening_socket_count; j++) {
                if(core_conf->listening_sockets[j].fd != -1) {
                    if(core_conf->listening_sockets[j].is_ssl) {
                        if (worker_https_fd == -1) worker_https_fd = core_conf->listening_sockets[j].fd;
                    } else {
                        if (worker_http_fd == -1) worker_http_fd = core_conf->listening_sockets[j].fd;
                    }
                }
            }
            worker_loop(worker_http_fd, worker_https_fd, ssl_ctx);
            exit(0);
        } else {
            worker_pids[i] = pid;
            num_workers_spawned++;
        }
    }
    log_message(LOG_LEVEL_DEBUG, "--> main: All workers forked");

    for (int i = 0; i < core_conf->worker_processes; i++) {
        wait(NULL);
    }

    log_message(LOG_LEVEL_INFO, "All workers have shut down. Master process exiting.");
    log_message(LOG_LEVEL_DEBUG, "--> main: Cleaning up...");
    
    for(int i = 0; i < core_conf->listening_socket_count; i++) {
        if(core_conf->listening_sockets[i].fd != -1)
            close(core_conf->listening_sockets[i].fd);
    }

    if (ssl_ctx) SSL_CTX_free(ssl_ctx);
    free_core_config(core_conf);
    free_config(g_config);

    log_message(LOG_LEVEL_DEBUG, "--> main: END");
    return 0;
}