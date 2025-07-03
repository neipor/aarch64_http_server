/*
 * ANX - Stage 1: Minimal Viable Core
 * A simple TCP server that listens on a port, accepts one connection,
 * sends a hardcoded response, and closes the connection.
 *
 * Architecture: AArch64 (ARMv8-A)
 * OS: Linux
 */

.section .data
    // HTTP Response parts
    http_status_line: .ascii "HTTP/1.0 200 OK\r\n"
    .equ http_status_line_len, . - http_status_line
    
    http_header_content_type: .ascii "Content-Type: text/plain\r\n"
    .equ http_header_content_type_len, . - http_header_content_type
    
    http_header_server: .ascii "Server: ANX/0.2.0\r\n"
    .equ http_header_server_len, . - http_header_server

    http_header_end: .ascii "\r\n"
    .equ http_header_end_len, . - http_header_end

    response_body_prefix: .ascii "You requested path: "
    .equ response_body_prefix_len, . - response_body_prefix

    response_body_suffix: .ascii "\r\n"
    .equ response_body_suffix_len, . - response_body_suffix

    // New debug response
    debug_response: .ascii "HTTP/1.0 200 OK\r\n\r\nParsing finished.\r\n"
    .equ debug_response_len, . - debug_response

    // --- DEBUG STRINGS ---
    dbg_accepted: .ascii "1. Accepted connection\n"
    .equ dbg_accepted_len, . - dbg_accepted
    dbg_read_ok: .ascii "2. Read successful\n"
    .equ dbg_read_ok_len, . - dbg_read_ok
    dbg_found_space1: .ascii "3. Found first space\n"
    .equ dbg_found_space1_len, . - dbg_found_space1
    dbg_found_space2: .ascii "4. Found second space\n"
    .equ dbg_found_space2_len, . - dbg_found_space2
    dbg_building_resp: .ascii "5. Starting response build\n"
    .equ dbg_building_resp_len, . - dbg_building_resp
    dbg_before_write: .ascii "6. Before final write\n"
    .equ dbg_before_write_len, . - dbg_before_write

.section .bss
    .lcomm request_buffer, 4096 // Reserve 4KB for the HTTP request
    .lcomm response_buffer, 4096 // Reserve 4KB for the outgoing HTTP response

.section .text
.global _start

// System call numbers for AArch64
.equ SYS_SOCKET, 198
.equ SYS_BIND, 200
.equ SYS_LISTEN, 201
.equ SYS_ACCEPT, 202
.equ SYS_WRITE, 64
.equ SYS_READ, 63
.equ SYS_CLOSE, 57
.equ SYS_EXIT, 93
.equ SYS_SETSOCKOPT, 208

.macro print_debug msg, len
    // Save ALL general-purpose argument/temporary registers to be safe.
    stp x0, x1, [sp, #-16]!
    stp x2, x3, [sp, #-16]!
    stp x4, x5, [sp, #-16]!
    stp x6, x7, [sp, #-16]!
    stp x8, x30, [sp, #-16]! // Also save syscall reg and link register

    // Issue the write syscall to stdout.
    mov x0, #1              // fd 1 = stdout
    ldr x1, =\msg
    mov x2, #\len
    mov x8, #SYS_WRITE
    svc #0

    // Restore the registers from the stack in reverse order.
    ldp x8, x30, [sp], #16
    ldp x6, x7, [sp], #16
    ldp x4, x5, [sp], #16
    ldp x2, x3, [sp], #16
    ldp x0, x1, [sp], #16
.endm

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

    // --- 1.5 Set SO_REUSEADDR to avoid "Address already in use" errors ---
    // We need a pointer to an integer with value 1 for optval
    // IMPORTANT: Stack pointer (sp) must be 16-byte aligned on AArch64.
    // We allocate 16 bytes even though we only need 4.
    sub sp, sp, #16         // Make 16 bytes of space on the stack
    mov w1, #1
    str w1, [sp]            // Store 1 on the stack
    mov x3, sp              // x3 = &optval

    mov x8, #SYS_SETSOCKOPT
    mov x0, x19             // server_fd from socket call
    mov x1, #1              // SOL_SOCKET level
    mov x2, #2              // SO_REUSEADDR option
    // x3 has the address of our optval (1)
    mov x4, #4              // optlen = sizeof(int)
    svc #0
    // We'll skip error checking for setsockopt for now.

    add sp, sp, #16         // Restore stack pointer

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

    // --- 5. Read the HTTP request from the client ---
    mov x8, #SYS_READ
    mov x0, x20             // client_fd
    ldr x1, =request_buffer
    mov x2, #4096
    svc #0
    // Check for read errors or closed connection
    cmp x0, #0
    b.le close_connection // If n_read <= 0, close and wait for next client

    // x0 holds n_read. Calculate end of buffer.
    ldr x4, =request_buffer
    add x4, x4, x0          // x4 = buffer_end

    // --- 6. Parse the request to find the path ---
    // Simple parsing: find first ' ' and second ' '
    // Path starts after the first space.
    ldr x1, =request_buffer   // x1 = path_start
    mov x2, #0              // x2 = path_len
    mov x3, x1              // x3 = current pointer for scanning
    
    // Find first space
    find_first_space:
        ldrb w4, [x3], #1
        cmp w4, #' '
        b.eq found_first_space
        cmp x3, x4 // Compare current pointer with buffer_end
        b.lt find_first_space
        // If no space found, it's a bad request. We'll handle this later.
        b close_connection

    found_first_space:
        mov x1, x3              // Path starts after the first space

    // Find second space
    find_second_space:
        ldrb w4, [x3], #1
        cmp w4, #' '
        b.eq found_second_space
        cmp x3, x4 // Compare current pointer with buffer_end
        b.lt find_second_space
        // If no second space, also a bad request.
        b close_connection
    
    found_second_space:
        sub x2, x3, x1          // path_len = end - start
        sub x2, x2, #1          // Don't include the space itself

    // --- 7. Construct the response ---
    ldr x5, =response_buffer // x5 = destination pointer
    
    // Copy Status Line
    ldr x6, =http_status_line
    mov x7, #http_status_line_len
    bl memcpy

    // Copy Headers
    ldr x6, =http_header_content_type
    mov x7, #http_header_content_type_len
    bl memcpy

    ldr x6, =http_header_server
    mov x7, #http_header_server_len
    bl memcpy

    ldr x6, =http_header_end
    mov x7, #http_header_end_len
    bl memcpy

    // Copy body prefix
    ldr x6, =response_body_prefix
    mov x7, #response_body_prefix_len
    bl memcpy

    // Copy the actual path from request_buffer
    mov x6, x1 // x1 is path_start from parsing
    mov x7, x2 // x2 is path_len from parsing
    bl memcpy

    // Copy body suffix
    ldr x6, =response_body_suffix
    mov x7, #response_body_suffix_len
    bl memcpy

    ldr x9, =response_buffer
    sub x9, x5, x9

    // --- 8. Write the constructed response ---
    mov x8, #SYS_WRITE
    mov x0, x20             // client_fd
    ldr x1, =response_buffer
    mov x2, x9              // Total length
    svc #0

close_connection:
    // --- 9. Close the client socket ---
    mov x8, #SYS_CLOSE
    mov x0, x20             // client_fd
    svc #0

    // For L2 demonstration, we will exit after one request to get a clean curl output.
    // In L3 (epoll), we will loop properly.
    b _exit

    // b accept_loop // This will be used in the next stage

_exit:
    // --- 9. Exit the program ---
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

memcpy:
    // Simple memcpy(dest, src, n)
    // x5 = dest, x6 = src, x7 = n
    // Corrupts x5, x6, x7, x9 (was x8, which is syscall reg!)
    1:
        ldrb w9, [x6], #1
        strb w9, [x5], #1
        subs x7, x7, #1
        b.ne 1b
    ret 