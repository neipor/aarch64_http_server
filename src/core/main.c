#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <dlfcn.h>
#include "../include/anx_rust.h"

#include "config.h"
#include "core.h"
#include "log.h"
#include "net.h"
#include "https.h"

#include <arpa/inet.h>
#include <errno.h>

pid_t worker_pids[128];
int num_workers_spawned = 0;
log_config_t *global_log_config = NULL;

int is_port_in_use(uint16_t port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 1;
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    int res = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    close(sock);
    return res < 0 && errno == EADDRINUSE;
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

int main(int argc, char *argv[]) {
    // 检查是否有新风格参数（--开头）
    int has_cli = 0;
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], "--", 2) == 0) {
            has_cli = 1;
            break;
        }
    }
    // 支持 --help 和 --version 和 --dry-run
    int dry_run = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0) {
            printf("\033[1;36mANX HTTP Server v1.1.0+\033[0m\n");
            printf("\033[1;32mUsage: %s [options]\033[0m\n", argv[0]);
            printf("  \033[1;33m--static-dir DIR\033[0m      指定静态文件目录\n");
            printf("  \033[1;33m--proxy PATH URL\033[0m      配置反向代理\n");
            printf("  \033[1;33m--port PORT\033[0m           监听端口 (默认8080)\n");
            printf("  \033[1;33m--host HOST\033[0m           监听主机 (默认0.0.0.0)\n");
            printf("  \033[1;33m--ssl-cert FILE\033[0m       SSL证书\n");
            printf("  \033[1;33m--ssl-key FILE\033[0m        SSL私钥\n");
            printf("  \033[1;33m--cache-size SIZE\033[0m     缓存大小\n");
            printf("  \033[1;33m--cache-ttl SECS\033[0m      缓存TTL\n");
            printf("  \033[1;33m--log-level LEVEL\033[0m     日志级别 (info/debug/warning/error)\n");
            printf("  \033[1;33m--log-file FILE\033[0m       日志文件\n");
            printf("  \033[1;33m--daemon\033[0m              守护进程模式\n");
            printf("  \033[1;33m--pid-file FILE\033[0m       PID文件\n");
            printf("  \033[1;33m--dry-run\033[0m             仅打印解析结果不启动服务\n");
            printf("  \033[1;33m--help\033[0m                显示帮助\n");
            printf("  \033[1;33m--version\033[0m             显示版本\n");
            printf("  \033[1;33m-c config_file\033[0m        使用配置文件\n");
            printf("\n\033[1;32m示例：\033[0m\n");
            printf("  ./anx --static-dir ./www --port 8080\n");
            printf("  ./anx --static-dir ./www --proxy /api http://localhost:3000 --port 80\n");
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0) {
            printf("ANX HTTP Server v1.1.0+\n");
            return 0;
        }
        if (strcmp(argv[i], "--dry-run") == 0) {
            dry_run = 1;
        }
    }
    if (has_cli) {
        // 使用Rust CLI FFI解析参数
        CliParser* parser = anx_cli_parser_create();
        if (!parser) {
            fprintf(stderr, "[FATAL] Failed to create CLI parser.\n");
            return 1;
        }
        // 传递argc/argv给Rust（如有需要可扩展anx_cli_parse_args接口）
        CliConfigHandle* cli_conf = anx_cli_parse_args(parser);
        if (!cli_conf) {
            fprintf(stderr, "[FATAL] Failed to parse CLI arguments.\n");
            anx_cli_parser_free(parser);
            return 1;
        }
        
        // 读取CLI参数
        uint16_t port = anx_cli_get_port(cli_conf);
        char* static_dir = anx_cli_get_static_dir(cli_conf);
        char* host = anx_cli_get_host(cli_conf);
        int is_daemon = anx_cli_is_daemon(cli_conf);
        char* log_level_str = anx_cli_get_log_level(cli_conf);
        char* log_file = anx_cli_get_log_file(cli_conf);
        int cache_enabled = anx_cli_is_cache_enabled(cli_conf);
        size_t cache_size = anx_cli_get_cache_size(cli_conf);
        unsigned long cache_ttl = anx_cli_get_cache_ttl(cli_conf);
        size_t threads = anx_cli_get_threads(cli_conf);
        size_t max_connections = anx_cli_get_max_connections(cli_conf);
        
        printf("[CLI] 启动参数: --static-dir=%s --port=%u --host=%s\n", 
               static_dir ? static_dir : "", port, host ? host : "");
        
        // 初始化日志系统
        log_level_t log_level = LOG_LEVEL_INFO;
        if (log_level_str) {
            if (strcmp(log_level_str, "debug") == 0) log_level = LOG_LEVEL_DEBUG;
            else if (strcmp(log_level_str, "warning") == 0) log_level = LOG_LEVEL_WARNING;
            else if (strcmp(log_level_str, "error") == 0) log_level = LOG_LEVEL_ERROR;
        }
        
        if (log_file && strcmp(log_file, "-") != 0) {
            log_init(log_file, log_level);
        } else {
            log_init("stderr", log_level);
        }
        
        log_message(LOG_LEVEL_INFO, "ANX HTTP Server v1.1.0+ starting up...");
        
        // 创建配置结构体
        config_t *config = calloc(1, sizeof(config_t));
        if (!config) {
            log_message(LOG_LEVEL_ERROR, "Failed to allocate configuration");
            anx_cli_config_free(cli_conf);
            anx_cli_parser_free(parser);
            return 1;
        }
        
        // 设置基本配置
        config->worker_processes = threads > 0 ? threads : 2;
        config->log_level = log_level;
        if (log_file) config->access_log = strdup(log_file);
        
        // 创建HTTP块配置
        config->http = calloc(1, sizeof(http_block_t));
        if (!config->http) {
            log_message(LOG_LEVEL_ERROR, "Failed to allocate HTTP block");
            free(config);
            anx_cli_config_free(cli_conf);
            anx_cli_parser_free(parser);
            return 1;
        }
        
        // 创建服务器块配置
        config->http->servers = calloc(1, sizeof(server_block_t));
        if (!config->http->servers) {
            log_message(LOG_LEVEL_ERROR, "Failed to allocate server block");
            free(config->http);
            free(config);
            anx_cli_config_free(cli_conf);
            anx_cli_parser_free(parser);
            return 1;
        }
        
        server_block_t *server = config->http->servers;
        
        // 设置监听端口
        server->directive_count = 1;
        server->directives = calloc(1, sizeof(directive_t));
        server->directives[0].key = strdup("listen");
        char listen_value[64];
        snprintf(listen_value, sizeof(listen_value), "%u", port);
        server->directives[0].value = strdup(listen_value);
        
        // 设置静态文件目录
        if (static_dir) {
            server->directive_count++;
            server->directives = realloc(server->directives, 
                                       sizeof(directive_t) * server->directive_count);
            server->directives[1].key = strdup("root");
            server->directives[1].value = strdup(static_dir);
        }
        
        // 设置缓存配置
        if (cache_enabled) {
            config->cache = cache_config_create();
            if (config->cache) {
                config->cache->enable_cache = 1;
                config->cache->max_size = cache_size;
                config->cache->default_ttl = cache_ttl;
                log_message(LOG_LEVEL_INFO, "Cache enabled");
            }
        }
        
        // 处理反向代理配置
        size_t proxy_count = anx_cli_get_proxy_count(cli_conf);
        if (proxy_count > 0) {
            server->locations = calloc(proxy_count, sizeof(location_block_t));
            for (size_t i = 0; i < proxy_count; i++) {
                char* proxy_path = anx_cli_get_proxy_path_prefix(cli_conf, i);
                char* proxy_url = anx_cli_get_proxy_url(cli_conf, i);
                
                if (proxy_url) {
                    location_block_t *loc = &server->locations[i];
                    // 如果proxy_path为NULL或空，默认为"/"
                    if (proxy_path && strlen(proxy_path) > 0) {
                        loc->path = strdup(proxy_path);
                    } else {
                        loc->path = strdup("/");
                    }
                    loc->directive_count = 1;
                    loc->directives = calloc(1, sizeof(directive_t));
                    loc->directives[0].key = strdup("proxy_pass");
                    loc->directives[0].value = strdup(proxy_url);
                    
                    char log_msg[256];
                    snprintf(log_msg, sizeof(log_msg), "Proxy configured: %s -> %s", loc->path, proxy_url);
                    log_message(LOG_LEVEL_INFO, log_msg);
                }
                
                anx_free_string(proxy_path);
                anx_free_string(proxy_url);
            }
        }
        
        // 创建核心配置
        core_config_t *core_conf = create_core_config(config);
        if (!core_conf) {
            log_message(LOG_LEVEL_ERROR, "Failed to create core configuration");
            free_config(config);
            anx_cli_config_free(cli_conf);
            anx_cli_parser_free(parser);
            return 1;
        }
        
        // 初始化SSL
        SSL_library_init();
        OpenSSL_add_all_algorithms();
        SSL_load_error_strings();
        SSL_CTX *ssl_ctx = NULL;
        
        // 检查SSL配置
        int is_ssl_enabled = anx_cli_is_ssl_enabled(cli_conf);
        if (is_ssl_enabled) {
            char* ssl_cert = anx_cli_get_ssl_cert_file(cli_conf);
            char* ssl_key = anx_cli_get_ssl_key_file(cli_conf);
            
            if (ssl_cert && ssl_key) {
                ssl_ctx = SSL_CTX_new(TLS_server_method());
                if (ssl_ctx) {
                    if (SSL_CTX_use_certificate_file(ssl_ctx, ssl_cert, SSL_FILETYPE_PEM) <= 0 ||
                        SSL_CTX_use_PrivateKey_file(ssl_ctx, ssl_key, SSL_FILETYPE_PEM) <= 0) {
                        log_message(LOG_LEVEL_ERROR, "Failed to load SSL certificate/key");
                        SSL_CTX_free(ssl_ctx);
                        ssl_ctx = NULL;
                    } else {
                        log_message(LOG_LEVEL_INFO, "SSL Context initialized successfully");
                    }
                }
            }
            
            anx_free_string(ssl_cert);
            anx_free_string(ssl_key);
        }
        
        // 创建监听socket
        int server_fd = create_server_socket(port);
        if (server_fd < 0) {
            log_message(LOG_LEVEL_ERROR, "Failed to create server socket");
            if (ssl_ctx) SSL_CTX_free(ssl_ctx);
            free_core_config(core_conf);
            free_config(config);
            anx_cli_config_free(cli_conf);
            anx_cli_parser_free(parser);
            return 1;
        }
        
        char msg[128];
        snprintf(msg, sizeof(msg), "HTTP server listening on port %u", port);
        log_message(LOG_LEVEL_INFO, msg);
        
        // 设置信号处理
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        
        // 启动worker进程
        snprintf(msg, sizeof(msg), "Master process starting %d workers...", core_conf->worker_processes);
        log_message(LOG_LEVEL_INFO, msg);
        
        for (int i = 0; i < core_conf->worker_processes; i++) {
            pid_t pid = fork();
            if (pid == -1) {
                perror("fork failed");
                cleanup_logging();
                exit(EXIT_FAILURE);
            } else if (pid == 0) {
                // Worker process
                worker_loop(server_fd, -1, core_conf, ssl_ctx);
                exit(0);
            } else {
                worker_pids[i] = pid;
                num_workers_spawned++;
            }
        }
        
        log_message(LOG_LEVEL_DEBUG, "All workers forked");
        
        // 等待worker进程
        for (int i = 0; i < core_conf->worker_processes; i++) {
            wait(NULL);
        }
        
        log_message(LOG_LEVEL_INFO, "All workers have shut down. Master process exiting.");
        
        // 清理资源
        close(server_fd);
        if (ssl_ctx) SSL_CTX_free(ssl_ctx);
        free_core_config(core_conf);
        free_config(config);
        cleanup_logging();
        
        // 清理CLI资源
        anx_free_string(static_dir);
        anx_free_string(host);
        anx_free_string(log_level_str);
        anx_free_string(log_file);
        anx_cli_config_free(cli_conf);
        anx_cli_parser_free(parser);
        
        // 启动成功高亮提示
        printf("\033[1;32m[OK] ANX HTTP Server 已启动！\033[0m\n");
        printf("访问入口: \033[1;36mhttp://%s:%u/\033[0m\n", host, port);
        printf("静态目录: %s\n线程数: %zu\n缓存: %s\n", static_dir, threads, cache_enabled ? "启用" : "关闭");
        fflush(stdout);
        
        return 0;
    }

    // Initialize basic logging first
    log_init("stderr", LOG_LEVEL_INFO);

    char *config_file = "server.conf"; // Default config path
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
            case 'c':
                config_file = optarg;
                break;
            default: /* '?' */
                fprintf(stderr, "Usage: %s [-c config_file]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    config_t *config = parse_config(config_file);
    if (config == NULL) {
        fprintf(stderr, "Failed to parse configuration from %s\n", config_file);
        return 1;
    }

    // Extract and initialize logging configuration
    global_log_config = extract_log_config(config);
    if (global_log_config) {
        init_logging_from_config(global_log_config);
        log_message(LOG_LEVEL_INFO, "ANX HTTP Server starting up...");
    } else {
        log_message(LOG_LEVEL_WARNING, "Failed to extract log configuration, using defaults");
    }

    // 2. Process the syntax tree into a usable core configuration
    core_config_t *core_conf = create_core_config(config);
    if (!core_conf) {
        log_message(LOG_LEVEL_ERROR, "Failed to process configuration. Exiting.");
        free_config(config);
        cleanup_logging();
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
            
            char log_buf[512];
            snprintf(log_buf, sizeof(log_buf), "Attempting to load SSL cert from: %s", cert_file);
            log_message(LOG_LEVEL_DEBUG, log_buf);
            snprintf(log_buf, sizeof(log_buf), "Attempting to load SSL key from: %s", key_file);
            log_message(LOG_LEVEL_DEBUG, log_buf);

            ssl_ctx = SSL_CTX_new(TLS_server_method());
            if (!ssl_ctx) {
                ERR_print_errors_fp(stderr);
                log_message(LOG_LEVEL_ERROR, "Failed to create SSL context");
                cleanup_logging();
                exit(EXIT_FAILURE);
            }

            if (SSL_CTX_use_certificate_file(ssl_ctx, cert_file,
                                             SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                log_message(LOG_LEVEL_ERROR, "Failed to load certificate file.");
                cleanup_logging();
                exit(EXIT_FAILURE);
            }
            if (SSL_CTX_use_PrivateKey_file(ssl_ctx, key_file,
                                            SSL_FILETYPE_PEM) <= 0) {
                ERR_print_errors_fp(stderr);
                log_message(LOG_LEVEL_ERROR, "Failed to load private key file.");
                cleanup_logging();
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
            cleanup_logging();
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
            worker_loop(worker_http_fd, worker_https_fd, core_conf, ssl_ctx);
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

    // Cleanup logging at the end
    cleanup_logging();

    log_message(LOG_LEVEL_DEBUG, "--> main: END");
    return 0;
}