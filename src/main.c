#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 64
#define WEB_ROOT "./www"
#define DEFAULT_PAGE "/index.html"
#define NOT_FOUND_PAGE "/404.html"

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

void handle_http_request(int client_socket) {
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);

    if (bytes_read <= 0) {
        if (bytes_read == -1 && errno != EAGAIN) {
            perror("read error");
        }
        close(client_socket);
        return;
    }

    char *method = strtok(buffer, " \t\r\n");
    char *req_path = strtok(NULL, " \t\r\n");

    if (!method || !req_path) {
        close(client_socket);
        return;
    }

    printf("Received request on fd %d: Method=%s, Path=%s\n", client_socket, method, req_path);
    
    // Security: prevent directory traversal
    if (strstr(req_path, "..")) {
        // For simplicity, just close connection on malicious-looking paths
        close(client_socket);
        return;
    }

    char file_path[BUFFER_SIZE];
    if (strcmp(req_path, "/") == 0) {
        snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, DEFAULT_PAGE);
    } else {
        snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, req_path);
    }

    struct stat file_stat;
    int file_fd = -1;
    int status_code = 200;

    if (stat(file_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        status_code = 404;
        snprintf(file_path, sizeof(file_path), "%s%s", WEB_ROOT, NOT_FOUND_PAGE);
        stat(file_path, &file_stat); // Get stats for the 404 page
    }

    file_fd = open(file_path, O_RDONLY);
    if (file_fd < 0) {
        // If even the 404 page can't be opened, something is very wrong.
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
             status_code, (status_code == 200) ? "OK" : "Not Found",
             mime_type, file_stat.st_size);

    write(client_socket, header, strlen(header));
    sendfile(client_socket, file_fd, NULL, file_stat.st_size);
    close(file_fd);
    close(client_socket);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr;
    
    int epoll_fd;
    struct epoll_event event, events[MAX_EVENTS];

    // Ignore SIGPIPE, which happens if a client disconnects unexpectedly.
    signal(SIGPIPE, SIG_IGN);

    // 1. Create socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        error_and_exit("socket");
    }

    // 2. Set SO_REUSEADDR to allow fast restarts
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        error_and_exit("setsockopt");
    }

    // 3. Bind socket to an address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        error_and_exit("bind");
    }

    // 4. Make the server socket non-blocking
    if (make_socket_non_blocking(server_fd) == -1) {
        error_and_exit("make_socket_non_blocking");
    }

    // 5. Listen for connections
    if (listen(server_fd, SOMAXCONN) < 0) {
        error_and_exit("listen");
    }

    // 6. Create epoll instance
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        error_and_exit("epoll_create1");
    }

    // 7. Add server socket to epoll
    event.data.fd = server_fd;
    event.events = EPOLLIN | EPOLLET; // Edge-Triggered for new connections
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1) {
        error_and_exit("epoll_ctl");
    }

    printf("ANX (C-StaticFile-Server) is listening on port %d\n", PORT);

    // 8. The Main Event Loop
    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN)))
            {
                // An error has occured on this fd, or the socket is not ready for reading
                fprintf(stderr, "epoll error on fd %d\n", events[i].data.fd);
                close(events[i].data.fd);
                continue;
            } 
            else if (server_fd == events[i].data.fd) 
            {
                // --- Event on the listening socket: new connection(s) ---
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t client_len = sizeof(client_addr);
                    client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
                    if (client_fd == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            // We have processed all incoming connections.
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }
                    
                    if (make_socket_non_blocking(client_fd) == -1) {
                        close(client_fd);
                        continue;
                    }
                    
                    // Add the new client socket to the epoll interest list
                    event.data.fd = client_fd;
                    event.events = EPOLLIN | EPOLLET; // Edge-triggered for client data
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) == -1) {
                        perror("epoll_ctl add client");
                        close(client_fd);
                    }
                }
            }
            else
            {
                // --- Event on a client socket: data is ready to be read ---
                handle_http_request(events[i].data.fd);
            }
        }
    }

    // This part is unreachable in the while(1) loop
    close(server_fd);
    return 0;
} 