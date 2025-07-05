# ANX HTTP Server

A high-performance, Nginx-inspired HTTP/HTTPS server written in C, designed to be lightweight, secure, and production-ready.

## 🚀 Current Status (v0.2.0)

ANX successfully implements core HTTP server functionality with modern architecture:

### ✅ Working Features
- **HTTP/1.1 and HTTPS** support with SSL/TLS encryption
- **Multi-process architecture** (master + worker processes)
- **Virtual hosts** with `server_name` directive support
- **Location-based routing** for flexible request handling
- **Static file serving** with automatic MIME type detection
- **Nginx-style configuration** syntax (`http {}`, `server {}`, `location {}`)
- **Epoll-based event handling** for high concurrency
- **"Green" deployment** - portable with configurable paths
- **Docker containerization** for development and testing
- **Automated testing** with comprehensive test suite

### 🎯 Performance
- **Multi-process concurrent handling** using epoll
- **Non-blocking I/O** for optimal performance
- **SSL/TLS optimization** with proper certificate handling
- **Memory-safe** implementation with proper resource cleanup

## 📋 Quick Start

### Prerequisites
- GCC compiler
- OpenSSL development libraries
- Make build system
- Docker (optional, for containerized development)

### Build and Run
```bash
# Clone the repository
git clone <repository-url>
cd asm_http_server

# Build the server
make clean && make

# Run with default configuration
./anx

# Run with custom configuration
./anx -c /path/to/your/config.conf

# Run automated tests
make test
```

### Docker Development
```bash
# Build development environment
make docker-build-dev

# Run in container
make docker-run-dev
```

## ⚙️ Configuration

ANX uses Nginx-compatible configuration syntax:

```nginx
http {
    workers 2;
    
    server {
        listen 80;
        server_name localhost;
        root www;
        index index.html;
    }
    
    server {
        listen 443 ssl;
        server_name localhost;
        root www;
        index index.html;
        ssl_certificate certs/server.crt;
        ssl_certificate_key certs/server.key;
        
        location /api {
            proxy_pass http://127.0.0.1:3000;
        }
        
        location /static {
            index index.html;
        }
    }
}
```

### Supported Directives
- `workers` - Number of worker processes
- `listen` - Port and protocol (HTTP/HTTPS)
- `server_name` - Virtual host matching
- `root` - Document root directory
- `index` - Default index files
- `ssl_certificate` / `ssl_certificate_key` - SSL/TLS certificates
- `proxy_pass` - Reverse proxy configuration (✅ **COMPLETE v0.3.0**)
- `add_header` - HTTP header manipulation (✅ **COMPLETE v0.4.0**)

## 🏗️ Architecture

### Process Model
```
Master Process
├── Worker Process 1 (epoll event loop)
├── Worker Process 2 (epoll event loop)
└── Worker Process N (epoll event loop)
```

### Request Flow
1. **Connection Accept** - Worker accepts client connection
2. **SSL Handshake** - Performs SSL negotiation (if HTTPS)
3. **Request Parsing** - Parses HTTP request headers
4. **Routing** - Matches request to server/location blocks
5. **Response Generation** - Serves static files or proxies to backend
6. **Connection Cleanup** - Properly closes connection and frees resources

### Module Structure
- `src/main.c` - Master process and worker management
- `src/config.c` - Configuration file parsing
- `src/core.c` - Core configuration processing and routing
- `src/net.c` - Network handling and event loops
- `src/http.c` - HTTP request/response handling
- `src/https.c` - HTTPS/SSL request handling
- `src/log.c` - Logging infrastructure
- `src/util.c` - Utility functions (MIME types, etc.)

## 🧪 Testing

ANX includes comprehensive automated testing:

```bash
# Run all tests with Docker
make test

# Manual testing
./anx -c server.conf &
curl http://localhost/
curl -k https://localhost/
```

The test suite validates:
- HTTP/HTTPS functionality
- SSL certificate loading
- Virtual host routing
- Static file serving
- Error handling
- Configuration parsing

## 🛣️ Development Roadmap

See [ROADMAP.md](ROADMAP.md) for detailed development plans.

### Phase 1: Core HTTP Features (Next)
- Reverse proxy implementation (`proxy_pass`)
- HTTP header manipulation
- Access logging infrastructure
- Gzip compression support

### Phase 2: Advanced Routing
- Regex location matching
- URL rewriting capabilities
- Custom error pages
- Directory auto-indexing

### Phase 3: Performance & Scalability
- HTTP Keep-Alive connections
- Load balancing algorithms
- Memory pool optimization
- Proxy caching system

### Long-term Goals
- 80%+ Nginx feature compatibility
- Production-ready performance (10K+ concurrent connections)
- Enterprise security features
- Plugin/module system

## 🤝 Contributing

We welcome contributions! Key areas for development:

1. **Core Features** - Implement reverse proxy, compression, advanced routing
2. **Performance** - Optimize memory usage and request handling
3. **Testing** - Expand test coverage and edge case handling
4. **Documentation** - User guides and API documentation
5. **Security** - Security audits and vulnerability testing

### Development Guidelines
- Follow existing C code style
- Add tests for new features
- Update documentation
- Ensure memory safety
- Maintain cross-platform compatibility

## 📊 Comparison with Nginx

| Feature | ANX v0.2.0 | Nginx | Status |
|---------|------------|-------|--------|
| HTTP/1.1 | ✅ | ✅ | Complete |
| HTTPS/SSL | ✅ | ✅ | Complete |
| Virtual Hosts | ✅ | ✅ | Complete |
| Static Files | ✅ | ✅ | Complete |
| Reverse Proxy | ✅ | ✅ | **NEW v0.3.0** |
| Header Manipulation | ✅ | ✅ | **NEW v0.4.0** |
| Load Balancing | ❌ | ✅ | Planned |
| Compression | ❌ | ✅ | Planned |
| HTTP/2 | ❌ | ✅ | Future |
| Modules | ❌ | ✅ | Future |

**Current Nginx Compatibility: ~20%**

## 📈 Performance Benchmarks

*Benchmarks coming soon - currently in development*

Target performance goals:
- **Throughput**: 50,000+ requests/second (static files)
- **Latency**: <1ms median response time
- **Memory**: <50MB base footprint
- **Concurrency**: 10,000+ simultaneous connections

## 📄 License

MIT License - see [LICENSE](LICENSE) file for details.

## 🔗 Links

- **Documentation**: [docs/](docs/) (coming soon)
- **Issue Tracker**: GitHub Issues
- **Roadmap**: [ROADMAP.md](ROADMAP.md)
- **Contributing**: [CONTRIBUTING.md](CONTRIBUTING.md) (coming soon)

---

**ANX HTTP Server** - Building the next generation of high-performance web servers.

*Last updated: 2025-07-05 | Version: 0.4.0* 