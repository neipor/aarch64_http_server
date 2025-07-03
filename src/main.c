#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 4096

// Function to handle errors and exit
void error_and_exit(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];
    
    // Ignore SIGPIPE, which happens if a client disconnects unexpectedly.
    // We'll handle write errors manually instead.
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

    // 4. Listen for connections
    if (listen(server_fd, 10) < 0) {
        error_and_exit("listen");
    }

    printf("ANX (C-Version) is listening on port %d\n", PORT);

    // 5. Main accept loop
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept"); // Don't exit on accept failure, just log and continue
            continue;
        }
        
        // --- Handle a single client connection ---
        
        // Clear buffer
        memset(buffer, 0, BUFFER_SIZE);

        // Read request from client
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            // Client closed connection or an error occurred
            close(client_fd);
            continue;
        }
        
        // --- Parse the HTTP request to get the path ---
        char *method = strtok(buffer, " \t\r\n");
        char *path = strtok(NULL, " \t\r\n");

        if (method == NULL || path == NULL) {
            // Malformed request
            close(client_fd);
            continue;
        }

        printf("Received request: Method=%s, Path=%s\n", method, path);

        // --- Build the HTTP response ---
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response),
                 "HTTP/1.1 200 OK\r\n"
                 "Content-Type: text/plain\r\n"
                 "Server: ANX-C/0.3.0\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n"
                 "You requested path: %s",
                 strlen(path) + strlen("You requested path: "), path);

        // --- Send the response ---
        if (write(client_fd, response, strlen(response)) < 0) {
            perror("write");
        }

        // --- Close the connection ---
        close(client_fd);
    }

    // This part is unreachable in the while(1) loop,
    // but good practice to have.
    close(server_fd);
    return 0;
} 