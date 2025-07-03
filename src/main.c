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

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_EVENTS 64

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

    printf("ANX (C-Epoll-Version) is listening on port %d\n", PORT);

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
                int client_socket = events[i].data.fd;
                char buffer[BUFFER_SIZE] = {0};
                
                ssize_t bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
                
                if (bytes_read == -1) {
                    // If EAGAIN, that means we have read all data. So do nothing.
                    // Otherwise, a real error occurred.
                    if (errno != EAGAIN) {
                        perror("read error");
                        close(client_socket);
                    }
                } else if (bytes_read == 0) {
                    // End of file. The remote has closed the connection.
                    close(client_socket);
                } else {
                    // --- Data was read successfully, process it ---
                    char *method = strtok(buffer, " \t\r\n");
                    char *path = strtok(NULL, " \t\r\n");

                    if (method && path) {
                         printf("Received request on fd %d: Method=%s, Path=%s\n", client_socket, method, path);

                        // Build the HTTP response
                        char response[BUFFER_SIZE];
                        snprintf(response, sizeof(response),
                                 "HTTP/1.1 200 OK\r\n"
                                 "Content-Type: text/plain\r\n"
                                 "Server: ANX-Epoll/0.4.0\r\n"
                                 "Connection: close\r\n" // Important: tell client we will close
                                 "Content-Length: %zu\r\n"
                                 "\r\n"
                                 "You requested path: %s",
                                 strlen(path) + strlen("You requested path: "), path);
                        
                        // Note: A non-blocking write() might not write all data at once.
                        // For this example, we assume it does. A robust implementation
                        // would handle partial writes.
                        if (write(client_socket, response, strlen(response)) < 0) {
                            perror("write");
                        }
                    }
                    // Regardless of success, we close the socket after one request.
                    // A more advanced server would handle keep-alive.
                    close(client_socket);
                }
            }
        }
    }

    // This part is unreachable in the while(1) loop
    close(server_fd);
    return 0;
} 