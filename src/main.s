/*
 * ANX - Stage 1: Minimal Viable Core
 * A simple TCP server that listens on a port, accepts one connection,
 * sends a hardcoded response, and closes the connection.
 *
 * Architecture: AArch64 (ARMv8-A)
 * OS: Linux
 */

.section .data
    // The hardcoded HTTP response.
    // HTTP/1.0 is simpler, no need to handle Host headers etc. for this stage.
    http_response:
        .ascii "HTTP/1.0 200 OK\r\n"
        .ascii "Content-Length: 17\r\n"
        .ascii "\r\n"
        .ascii "Hello from ASM!\r\n"

    // Calculate the length of the response string.
    .equ response_len, . - http_response

.section .text
.global _start

// System call numbers for AArch64
.equ SYS_SOCKET, 198
.equ SYS_BIND, 200
.equ SYS_LISTEN, 201
.equ SYS_ACCEPT, 202
.equ SYS_WRITE, 64
.equ SYS_CLOSE, 57
.equ SYS_EXIT, 93

// Socket constants
.equ AF_INET, 2        // IPv4
.equ SOCK_STREAM, 1    // TCP

// Server configuration
.equ PORT, 8080        // Port to listen on
.equ BACKLOG, 10       // Max pending connections

_start:
    // --- 1. Create a socket ---
    // socket(AF_INET, SOCK_STREAM, 0)
    mov x8, #SYS_SOCKET
    mov x0, #AF_INET
    mov x1, #SOCK_STREAM
    mov x2, #0              // Protocol (0 for default)
    svc #0
    // On success, x0 holds the new server file descriptor.
    // Check for errors (negative return value).
    cmp x0, #0
    b.lt _exit_error_no_socket
    // We store it in x19 to preserve it across other calls.
    mov x19, x0             // x19 = server_fd

    // --- 2. Bind the socket to an address and port ---
    // We need to create a sockaddr_in struct on the stack.
    // struct sockaddr_in {
    //   sa_family_t    sin_family; /* address family: AF_INET */
    //   in_port_t      sin_port;   /* port in network byte order */
    //   struct in_addr sin_addr;   /* internet address */
    // };
    // Internet address (sin_addr) is a struct with one field, s_addr.
    // All zero for INADDR_ANY.
    // The struct is 16 bytes long.
    sub sp, sp, #16
    mov x1, sp              // x1 now points to the struct on the stack

    // sin_port = htons(PORT). 8080 = 0x1F90. Big-endian = 0x901F.
    mov w2, #0x901F
    // sin_family = AF_INET
    mov w3, #AF_INET
    strh w2, [x1, #2]       // Store port at offset 2
    strh w3, [x1, #0]       // Store family at offset 0

    // sin_addr.s_addr = 0 (INADDR_ANY)
    str wzr, [x1, #4]       // Store 4 bytes of 0 at offset 4
    // sin_zero padding
    str xzr, [x1, #8]       // Store 8 bytes of 0 at offset 8

    // bind(server_fd, &sockaddr, sizeof(sockaddr))
    mov x8, #SYS_BIND
    mov x0, x19             // server_fd
    // x1 already points to the struct
    mov x2, #16             // sizeof(struct sockaddr_in)
    svc #0
    cmp x0, #0
    b.lt _exit_error_with_socket

    // Restore the stack pointer
    add sp, sp, #16

    // --- 3. Listen for incoming connections ---
    // listen(server_fd, backlog)
    mov x8, #SYS_LISTEN
    mov x0, x19             // server_fd
    mov x1, #BACKLOG
    svc #0
    cmp x0, #0
    b.lt _exit_error_with_socket

accept_loop:
    // --- 4. Accept a connection ---
    // This will block until a client connects.
    // accept(server_fd, NULL, NULL)
    mov x8, #SYS_ACCEPT
    mov x0, x19             // server_fd
    mov x1, #0              // Don't need client address
    mov x2, #0
    svc #0
    cmp x0, #0
    b.lt _exit_error_with_socket
    // On success, x0 holds the new client file descriptor.
    mov x20, x0             // x20 = client_fd

    // --- 5. Write the response to the client ---
    // write(client_fd, http_response, response_len)
    mov x8, #SYS_WRITE
    mov x0, x20             // client_fd
    ldr x1, =http_response
    mov x2, #response_len
    svc #0

    // --- 6. Close the client socket ---
    // close(client_fd)
    mov x8, #SYS_CLOSE
    mov x0, x20             // client_fd
    svc #0

    // For stage 1, we handle one request then exit.
    // In later stages, we will loop back to accept_loop.
    // jmp accept_loop

    // --- 7. Close the listening socket ---
    mov x8, #SYS_CLOSE
    mov x0, x19             // server_fd
    svc #0

_exit:
    // --- 8. Exit the program ---
    mov x8, #SYS_EXIT
    mov x0, #0              // Exit code 0
    svc #0

// --- Error Handling routines ---

// Exit with code 1 if socket creation fails.
_exit_error_no_socket:
    mov x8, #SYS_EXIT
    mov x0, #1
    svc #0

// Close the main socket, then exit with code 2.
// Used for errors after socket is created (bind, listen, accept).
_exit_error_with_socket:
    mov x8, #SYS_CLOSE
    mov x0, x19             // server_fd to close
    svc #0

    mov x8, #SYS_EXIT
    mov x0, #2              // Exit code 2
    svc #0 