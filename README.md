# ANX HTTP Server

ANX is a high-performance HTTP/HTTPS server written in C, designed to be fast, secure, and feature-rich while maintaining simplicity and ease of use.

## Features

- **HTTP/HTTPS Support**
  - HTTP/1.1 protocol support
  - HTTPS with SSL/TLS
  - Virtual hosts
  - Location-based routing

- **Performance**
  - Multi-process architecture
  - Epoll-based event handling
  - Keep-alive connections
  - Static file serving optimization
  - Content compression with gzip
  - Bandwidth limiting for large files (NEW in v0.8.0)

- **Reverse Proxy & Load Balancing**
  - Backend connection pooling
  - Advanced load balancing with 6 algorithms (NEW in v0.8.0)
  - Upstream health checks
  - Session persistence
  - Automatic failover

- **Streaming & Real-time (NEW in v0.8.0)**
  - Chunked transfer encoding
  - Server-Sent Events (SSE) push notifications
  - Stream module for TCP/UDP proxy
  - Real-time connection management

- **Security**
  - SSL/TLS support
  - Security headers
  - Access control
  - Rate limiting (coming soon)

- **Logging & Monitoring**
  - Access log with multiple formats
  - Error log with levels
  - Performance metrics
  - Log rotation

- **Content Optimization**
  - Gzip compression
  - Static file caching
  - MIME type detection
  - Conditional requests

## Installation

### Prerequisites

- Linux system (tested on Ubuntu 20.04+)
- GCC compiler
- Make build system
- OpenSSL development libraries
- zlib development libraries

```bash
# Install dependencies on Ubuntu/Debian
sudo apt-get update
sudo apt-get install gcc make libssl-dev zlib1g-dev

# Clone the repository
git clone https://github.com/yourusername/anx-http-server.git
cd anx-http-server

# Build the server
make clean && make

# Run the server
./anx -c server.conf
```

## Configuration

ANX uses an Nginx-style configuration format. Here's a basic example:

```nginx
http {
    # Basic settings
    workers 4;
    error_log ./logs/error.log;
    access_log ./logs/access.log;
    
    # Compression settings
    gzip on;
    gzip_comp_level 6;
    gzip_min_length 1024;
    gzip_types text/plain text/css text/javascript application/javascript 
              application/json application/xml text/xml application/x-javascript
              text/html;
    gzip_vary on;
    gzip_buffers 32 4k;
    
    server {
        listen 8080;
        server_name localhost;
        
        location / {
            root ./www;
        }
        
        location /api {
            proxy_pass http://localhost:3000;
        }
    }
}
```

## Testing

ANX comes with comprehensive test suites:

```bash
# Run all tests
./run_tests.sh

# Test specific features
./test_compression_demo.sh  # Test compression
./test_logging_demo.sh      # Test logging
./test_headers_demo.sh      # Test headers
```

## Documentation

- [Configuration Guide](docs/configuration.md)
- [API Documentation](docs/api.md)
- [Security Guide](docs/security.md)
- [Performance Tuning](docs/performance.md)

## Version History

- **v0.8.0** - Streaming & Real-time features (2024-12-19)
  - Chunked transfer encoding support
  - Bandwidth limiting with token bucket algorithm
  - Server-Sent Events (SSE) push notifications
  - Stream module for TCP/UDP load balancing
  - Real-time connection management
- **v0.7.0** - Cache system implementation (2024-12-19)
  - Memory caching with LRU/LFU/FIFO strategies
  - HTTP caching protocol support (ETag, Last-Modified)
  - Cache statistics and monitoring
- **v0.6.0** - Load balancing system (2024-12-19)
  - Upstream server groups
  - 6 load balancing algorithms
  - Health check and automatic failover
  - Session persistence
- **v0.5.0** - Content compression support (2025-01-06)
- **v0.4.0** - Access logging system (2025-01-05)
- **v0.3.0** - HTTP header manipulation (2025-01-04)
- **v0.2.0** - Reverse proxy implementation (2025-01-03)
- **v0.1.0** - Basic HTTP/HTTPS server (2025-01-02)

## Contributing

We welcome contributions! Please see our [Contributing Guide](CONTRIBUTING.md) for details.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- The Nginx project for inspiration
- The OpenSSL team for SSL/TLS support
- The zlib team for compression support
- All our contributors and users

## Contact

- GitHub Issues: [Project Issues](https://github.com/yourusername/anx-http-server/issues)
- Email: your.email@example.com
- Twitter: [@ANXServer](https://twitter.com/ANXServer) 