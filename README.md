# ANX - A(sm) Nginx-like Experimental Server

ANX is a lightweight, high-performance static file server designed to be both a learning project and a functional server. It started as an experiment in C and is planned to be gradually optimized with ARM assembly for maximum performance on Raspberry Pi and other ARM-based devices.

## Current State

The server is currently implemented in C and supports:
- Multi-worker architecture using `fork()`
- Event-driven I/O with `epoll`
- HTTP and HTTPS (with OpenSSL)
- Static file serving
- Configurable via `server.conf`
- Detailed logging

## Roadmap

The future development of ANX will follow these key phases:

1.  **Phase 1: C Implementation (Current)**
    - Build a robust and functional HTTP/S server in C.
    - Implement core features like static file serving, multi-worker processing, and basic configuration.
    - Ensure stability and correctness.

2.  **Phase 2: Gradual Assembly Optimization**
    - Identify performance-critical sections of the C code (e.g., request parsing, I/O handling).
    - Begin rewriting these specific functions in ARM assembly language.
    - Integrate assembly functions with the existing C codebase.
    - The goal is to profile and optimize, not to rewrite everything at once.

3.  **Phase 3: Advanced Features**
    - Implement more advanced Nginx-like features, such as reverse proxying, load balancing, and more complex routing.
    - The configuration for these features will aim for compatibility with Nginx's syntax where possible.

## Building and Running

1.  **Install dependencies** (like OpenSSL):
    ```bash
    sudo apt-get update && sudo apt-get install libssl-dev
    ```
2.  **Generate self-signed certificates** (for HTTPS):
    ```bash
    mkdir certs
    openssl req -x509 -newkey rsa:2048 -nodes -keyout certs/server.key -out certs/server.crt -subj "/C=CN/ST=BeiJing/L=BeiJing/O=asm_http_server/OU=dev/CN=localhost"
    ```
3.  **Compile**:
    ```bash
    make
    ```
4.  **Run**:
    ```bash
    ./anx server.conf
    ```

The server will then be listening on the ports defined in `server.conf`. 