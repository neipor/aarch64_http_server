# ANX Server

**ANX** is a high-performance, lightweight, and modern web server written in C, designed to be a learning tool and a powerful alternative to Nginx. Originally a project to explore ARM assembly optimizations, ANX has evolved into a full-featured server with a focus on clean code, modern practices, and ease of use.

## Features

- **Nginx-Compatible Configuration**: Uses a familiar, block-based configuration syntax.
- **HTTP/1.1 and HTTPS**: Supports both standard and encrypted connections out of the box.
- **Multi-Process Architecture**: Leverages a master-worker model for stability and performance.
- **High-Performance I/O**: Uses `epoll` for efficient handling of concurrent connections.
- **Containerized**: Fully containerized with Docker for easy, reproducible builds and deployment.
- **Modular Codebase**: Clean, well-organized source code for easy learning and contribution.

## Quick Start (Docker)

The easiest way to get ANX running is with Docker.

1.  **Build the Docker image:**
    ```bash
    make docker-build
    ```

2.  **Run the server:**
    ```bash
    make docker-run
    ```

    The server will be available at:
    - **HTTP**: `http://localhost:8080`
    - **HTTPS**: `https://localhost:8443` (You will need to accept the self-signed certificate)
    - **HTTP (alt)**: `http://localhost:9090`

## Building from Source

If you prefer to build from source directly on your machine:

1.  **Install dependencies:**
    ```bash
    # On Debian/Ubuntu
    sudo apt-get update
    sudo apt-get install -y build-essential libssl-dev
    ```

2.  **Compile the server:**
    ```bash
    make
    ```

3.  **Run the server:**
    > Note: Running without `sudo` will likely fail to bind to ports 80 and 443. You can edit `server.conf` to use higher ports (>1024).
    ```bash
    ./anx server.conf
    ```

## Contributing

Contributions are welcome! Please feel free to open an issue or submit a pull request. We follow a standard `develop` branch workflow. All new features should be developed in a `feature/*` branch and merged into `develop`. 