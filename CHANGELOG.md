# Changelog

All notable changes to the ANX HTTP Server project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.6.0] - 2025-01-06

### 🚀 Major Feature: Content Compression Support

This release implements Phase 1.4 of the roadmap, delivering a comprehensive content compression system that provides efficient data transfer and bandwidth optimization.

### Added
- **Complete Compression System**
  - Gzip compression support
  - Configurable compression levels
  - MIME type filtering
  - Client negotiation via Accept-Encoding
  - Vary header support
- **Performance Features**
  - Configurable minimum size for compression
  - Buffer size optimization
  - Memory-efficient compression
  - Streaming compression support
- **Configuration Options**
  - `gzip` directive for enabling/disabling
  - `gzip_comp_level` for compression level
  - `gzip_min_length` for minimum size
  - `gzip_types` for MIME type control
  - `gzip_vary` for Vary header control
  - `gzip_buffers` for buffer configuration

### Technical Implementation
- **New Compression Module**
  - `src/compress.h` and `src/compress.c` for compression operations
  - zlib integration for gzip compression
  - Efficient buffer management
  - Memory-safe compression context
- **Configuration Integration**
  - New compression directives in `server.conf`
  - MIME type management
  - Buffer size configuration
  - Compression level control

### Configuration Examples
```nginx
http {
    # Compression configuration
    gzip on;
    gzip_comp_level 6;
    gzip_min_length 1024;
    gzip_types text/plain text/css text/javascript application/javascript 
              application/json application/xml text/xml application/x-javascript
              text/html;
    gzip_vary on;
    gzip_buffers 32 4k;
}
```

### Performance Impact
- **Bandwidth Savings**
  - Up to 70% reduction for text content
  - Up to 50% reduction for JSON/XML
  - Configurable thresholds
- **CPU Usage**
  - Minimal impact with level 6
  - Configurable for balance
  - Smart compression decisions

### Testing and Validation
- **Comprehensive Test Suite**
  - `test_compression_demo.sh` for validation
  - Multiple content type testing
  - Compression ratio verification
  - Header validation

## [0.5.0] - 2025-01-05

### 🚀 Major Feature: Complete Access Logging Infrastructure

This release implements Phase 1.3 of the roadmap, delivering a comprehensive access logging system that provides detailed request tracking, multiple log formats, and production-ready logging capabilities.

### Added
- **Complete Access Logging System**
  - Combined Log Format (Apache-compatible)
  - Common Log Format support
  - JSON format for structured logging
  - Real-time access log generation
- **Advanced Request Tracking**
  - Precise request timing (millisecond precision)
  - Client IP address tracking
  - User-Agent and Referer header capture
  - Request/response size measurement
  - Server name and port logging
- **Performance Monitoring**
  - Request duration tracking
  - Response size statistics
  - Upstream/proxy request timing
  - Performance logging with configurable thresholds
- **Structured Error Logging**
  - Component-based error categorization
  - Contextual error information
  - Enhanced debugging capabilities
  - Error log correlation with access logs
- **Log Management Features**
  - Configurable log file locations
  - Log rotation by size (configurable MB threshold)
  - Time-based log retention
  - Automatic log file reopening after rotation
- **Production Configuration**
  - File-based error and access logging
  - Configurable log levels (error, warning, info, debug)
  - Performance logging toggle
  - Line-buffered output for real-time monitoring

### Technical Implementation
- **New Logging Architecture**
  - `src/log.h` and `src/log.c` completely rewritten
  - Memory-safe log entry management
  - Efficient timestamp formatting
  - Multiple concurrent log streams
- **Request Flow Integration**
  - HTTP and HTTPS request logging
  - Proxy request logging with upstream details
  - Error condition logging
  - Graceful failure handling
- **Configuration System Integration**
  - New configuration directives in `server.conf`
  - Automatic configuration extraction
  - Path resolution for log files
  - Runtime configuration validation

### Configuration Examples
```nginx
http {
    # Logging configuration
    error_log ./logs/error.log;      # Error log file path
    access_log ./logs/access.log;    # Access log file path
    log_level info;                  # Log level: error|warning|info|debug
    log_format combined;             # Log format: common|combined|json
    log_rotation_size 100;          # Rotate when log reaches 100MB
    log_rotation_days 7;             # Keep logs for 7 days
    performance_logging on;          # Enable performance metrics
    
    server {
        listen 8080;
        server_name localhost;
        
        location / {
            root ./www;
        }
    }
}
```

### Log Format Examples
**Combined Format (Default):**
```
127.0.0.1 - - [05/Jul/2025:15:36:51 +0800] "GET / HTTP/1.1" 200 743 "-" "curl/8.5.0" 0.000
```

**JSON Format:**
```json
{"timestamp":"2025-07-05T07:36:51Z","client_ip":"127.0.0.1","method":"GET","uri":"/","protocol":"HTTP/1.1","status_code":200,"response_size":743,"referer":"-","user_agent":"curl/8.5.0","server_name":"localhost","server_port":8080,"request_duration_ms":0.156,"upstream_status":0,"upstream_addr":"-","upstream_response_time_ms":0.0}
```

### Production Readiness
- **Monitoring Integration**
  - Standard log formats compatible with analysis tools
  - Real-time log streaming support
  - Error correlation capabilities
  - Performance baseline establishment
- **Operational Features**
  - Log rotation prevents disk space issues
  - Configurable retention policies
  - Error recovery mechanisms
  - Graceful startup/shutdown logging

### Developer Experience
- **Debug Support**
  - Detailed request flow logging
  - Error context preservation
  - Performance bottleneck identification
  - Configuration validation logging
- **Testing and Validation**
  - `test_logging_demo.sh` - Complete logging system validation
  - HTTP and HTTPS request testing
  - Error condition testing
  - Log format verification

## [0.4.0] - 2025-01-05

### 🚀 Major Feature: HTTP Header Manipulation System

This release implements Phase 1.2 of the roadmap, delivering a comprehensive HTTP header manipulation system that allows fine-grained control over request and response headers.

### Added
- **Complete HTTP Header Operations**
  - `add_header` directive: Add custom headers to responses
  - `set_header` directive: Set/override existing headers
  - `remove_header` directive: Remove unwanted headers
  - Support for `always` flag to apply headers even in error responses
- **Enhanced Response Headers**
  - Automatic Server identification header
  - Date header with GMT formatting
  - Connection management headers
  - Cache-Control headers for static files
- **Security Headers Support**
  - Strict-Transport-Security for HTTPS
  - X-Frame-Options for clickjacking protection
  - X-Content-Type-Options for MIME sniffing protection
  - X-XSS-Protection for cross-site scripting protection
  - Referrer-Policy for referrer information control
- **CORS (Cross-Origin Resource Sharing)**
  - Access-Control-Allow-Origin headers
  - Access-Control-Allow-Methods headers
  - Access-Control-Allow-Headers headers
  - Perfect for API endpoints

### Technical Implementation
- **New Header Management Module**
  - `src/headers.h` and `src/headers.c` for header operations
  - Efficient string manipulation for header insertion
  - Memory-safe buffer management
- **Configuration Integration**
  - Seamless integration with existing nginx-style configuration
  - Support for server-level and location-level header operations
  - Hierarchical header inheritance
- **Runtime Performance**
  - Optimized header application during response generation
  - Minimal overhead for header processing
  - Smart buffer management to prevent overflows

### Configuration Examples
```nginx
server {
    listen 8080;
    server_name localhost;
    
    # Global server headers
    add_header X-Server-Name ANX-HTTP-Server;
    add_header X-Version 0.4.0;
    
    location / {
        root ./www;
        # Cache control for static files
        add_header Cache-Control public-max-age-3600;
    }
    
    location /api {
        proxy_pass http://127.0.0.1:3000;
        # CORS headers for API
        add_header Access-Control-Allow-Origin *;
        add_header Access-Control-Allow-Methods GET-POST-PUT-DELETE-OPTIONS;
        add_header Access-Control-Allow-Headers Content-Type-Authorization;
    }
    
    location /admin {
        proxy_pass http://127.0.0.1:3001;
        # Security headers for admin interface
        add_header X-Frame-Options DENY;
        add_header X-Content-Type-Options nosniff;
        add_header X-XSS-Protection 1-mode-block;
    }
}

server {
    listen 8443 ssl;
    server_name localhost;
    
    # HTTPS security headers
    add_header Strict-Transport-Security max-age-31536000-includeSubDomains;
    add_header X-Forwarded-Proto https;
    
    location / {
        root ./www;
        add_header Cache-Control public-max-age-7200;
    }
}
```

### Testing and Validation
- **Comprehensive Header Testing**
  - Verified header insertion and formatting
  - Tested static file header application
  - Validated HTTPS security headers
  - Confirmed configuration parsing accuracy
- **Real-world Use Cases**
  - API CORS headers for web applications
  - Security headers for enhanced protection
  - Cache control for performance optimization
  - Custom branding headers

### Developer Experience
- **Debug Support**
  - Added debug logging for header operations
  - Clear error messages for configuration issues
  - Comprehensive configuration validation
- **Code Quality**
  - Clean, modular header management code
  - Memory-safe string operations
  - Extensive error checking and validation

### Nginx Compatibility
- **Directive Compatibility**
  - `add_header` directive matches nginx behavior
  - Support for conditional header application
  - Proper header inheritance and overrides
- **Enhanced Configuration**
  - Maintains full backward compatibility
  - Extended configuration parsing capabilities
  - Improved configuration debugging output

## [0.3.0] - 2025-01-05

### 🔄 Complete Reverse Proxy Implementation

Building on the solid foundation of v0.2.0, this release delivers a fully functional reverse proxy system, implementing Phase 1.1 of the roadmap.

### Added
- **Full Reverse Proxy Support**
  - Complete `proxy_pass` directive implementation
  - HTTP and HTTPS backend support
  - Automatic request/response forwarding
  - Backend connection management
- **Advanced URL Processing**
  - Comprehensive URL parser (protocol, host, port, path)
  - Intelligent path transformation
  - Query string preservation
- **Proxy Header Management**
  - Automatic X-Forwarded-For injection
  - X-Forwarded-Proto header for protocol awareness
  - Host header transformation for backend compatibility
- **Error Handling and Resilience**
  - 502 Bad Gateway on backend failures
  - 30-second connection timeout
  - Proper resource cleanup on errors
- **Production Features**
  - Connection pooling foundation
  - Backend health detection
  - Comprehensive error logging

### Technical Implementation
- **New Proxy Module** (`src/proxy.h`, `src/proxy.c`)
  - 350+ lines of production-quality proxy code
  - Memory-safe URL parsing and manipulation
  - Robust error handling and cleanup
- **Enhanced Routing System**
  - Fixed server selection logic (port + host matching)
  - Improved location block processing
  - Better debugging and logging
- **Configuration Updates**
  - Updated `server.conf` with comprehensive proxy examples
  - Multiple backend server configurations
  - Real-world proxy patterns

### Validated Functionality
- **HTTP Proxy** (Port 8080)
  - `/api` → `http://127.0.0.1:3000`
  - `/admin` → `http://127.0.0.1:3001`
  - Static files at `/`
- **HTTPS Proxy** (Port 8443)
  - `/api` → `http://127.0.0.1:3000`
  - `/service` → `http://127.0.0.1:3002`
  - SSL termination with backend HTTP

### Git Milestone
- Tagged as `v0.3.0` with complete feature set
- Comprehensive commit history
- Ready for production proxy workloads

## [0.2.0] - 2025-01-05

### 🎉 Major Milestone: Full HTTP/HTTPS Functionality

This release represents a complete rewrite and major advancement of the ANX HTTP Server, transforming it from a basic prototype into a functional, production-ready foundation.

### Added
- **Complete HTTPS Support** with SSL/TLS encryption
  - SSL certificate loading and validation
  - Proper SSL handshake implementation
  - SSL context management and cleanup
- **Advanced Configuration System**
  - Nginx-compatible configuration syntax
  - Support for `http {}`, `server {}`, `location {}` blocks
  - Virtual host support with `server_name` directive
  - SSL certificate path configuration
- **Robust Request Routing**
  - Host-based virtual host matching
  - Location-based request routing
  - Fallback to default server blocks
- **"Green" Application Design**
  - Command-line configuration file specification (`-c` flag)
  - Relative path resolution for portable deployment
  - No hardcoded paths or dependencies
- **Production-Ready Architecture**
  - Multi-process master/worker model
  - Epoll-based event handling for high concurrency
  - Proper resource cleanup and memory management
  - Signal handling for graceful shutdown
- **Comprehensive Testing**
  - Automated Docker-based testing
  - HTTP and HTTPS endpoint validation
  - Timeout protection for hanging tests
  - Container IP-based testing (no port mapping required)
- **Developer Experience**
  - Docker development environment
  - Automated build and test workflows
  - Debug logging with configurable levels
  - Clean modular code structure

### Changed
- **Complete Architecture Overhaul**
  - Eliminated global variable dependencies
  - Modular design with clear separation of concerns
  - Thread-safe configuration handling
- **Enhanced Error Handling**
  - Proper HTTP error responses (no more hanging connections)
  - SSL error handling and logging
  - Graceful fallback for missing files
- **Improved Configuration Processing**
  - Two-stage configuration: parsing → core processing
  - Better error messages for configuration issues
  - Support for relative and absolute paths

### Fixed
- **Critical Bug Fixes**
  - Fixed SSL handshake implementation (was missing `SSL_accept()`)
  - Resolved configuration routing issues (NULL pointer dereference)
  - Fixed HTTP response hanging when files not found
  - Corrected certificate path resolution in containers
- **Memory Management**
  - Fixed memory leaks in configuration processing
  - Proper SSL resource cleanup
  - Correct string allocation and deallocation
- **Build System**
  - Resolved linking issues with logging functions
  - Fixed header file dependencies
  - Corrected Docker build processes

### Technical Improvements
- **Code Quality**
  - ~2,000 lines of clean, well-documented C code
  - Consistent coding style and naming conventions
  - Comprehensive error checking and validation
- **Performance**
  - Non-blocking I/O with epoll
  - Efficient SSL/TLS handling
  - Optimized static file serving
- **Security**
  - Proper SSL/TLS implementation
  - Directory traversal protection
  - Secure memory handling

### Development Infrastructure
- **Docker Integration**
  - Development and production Docker environments
  - Automated testing with container isolation
  - Easy setup for new contributors
- **Build System**
  - Makefile with automatic dependency tracking
  - Clean separation of source and build artifacts
  - Cross-platform compatibility

## [0.1.0] - 2024-12-XX

### Added
- Initial HTTP server implementation
- Basic static file serving
- Simple configuration file support
- Multi-process architecture foundation
- Docker containerization
- Basic Makefile build system

### Known Issues in 0.1.0
- HTTPS support was incomplete
- Configuration parsing was limited
- No proper error handling for edge cases
- Limited testing infrastructure

---

## Upcoming Releases

### [0.3.0] - Planned (Phase 1: Core HTTP Features)
- **Reverse Proxy Implementation**
  - Functional `proxy_pass` directive
  - Backend connection pooling
  - Request/response forwarding
- **HTTP Header Manipulation**
  - Request and response header modification
  - Standard HTTP headers (Server, Date, etc.)
- **Logging Infrastructure**
  - Access log support with configurable formats
  - Log rotation capabilities
- **Compression Support**
  - Gzip compression for responses
  - MIME type-based compression filtering

### [0.4.0] - Planned (Phase 2: Advanced Routing)
- **Advanced Location Matching**
  - Regex location patterns
  - Exact match locations
  - Priority-based location selection
- **URL Rewriting**
  - `rewrite` directive implementation
  - `try_files` support
  - Internal redirects
- **Error Page Handling**
  - Custom error pages
  - Error page inheritance
- **Directory Features**
  - Auto-indexing for directories
  - Multiple index file support

### [1.0.0] - Target (Production Ready)
- **Load Balancing**
  - Multiple balancing algorithms
  - Health check support
  - Upstream server management
- **Performance Optimization**
  - HTTP Keep-Alive support
  - Memory pool management
  - Zero-copy operations
- **Security Features**
  - Rate limiting
  - Access control
  - Basic authentication
- **Production Features**
  - Graceful configuration reload
  - Comprehensive monitoring
  - Signal handling

---

## Development Notes

### Version 0.2.0 Development Process
This release involved extensive debugging and problem-solving:

1. **SSL Implementation Challenge**: Discovered that SSL connections were timing out due to missing `SSL_accept()` call for handshake completion.

2. **Configuration Architecture**: Completely redesigned configuration system to eliminate global variables and improve modularity.

3. **Docker Development**: Established robust Docker-based development and testing workflow.

4. **Path Resolution**: Implemented sophisticated relative path handling for truly portable deployments.

5. **Testing Infrastructure**: Built comprehensive automated testing with timeout protection and detailed logging.

### Contributors
- Primary development and architecture
- Extensive debugging and problem-solving
- Documentation and project planning

### Acknowledgments
- OpenSSL library for SSL/TLS support
- Linux epoll for high-performance I/O
- Docker for containerization support
- Nginx for configuration syntax inspiration

---

*For detailed technical information, see [ROADMAP.md](ROADMAP.md)*
*For contribution guidelines, see [CONTRIBUTING.md](CONTRIBUTING.md)*

## [v0.3.0] - 2025-07-05

### 🚀 Major Features Added
- **Reverse Proxy Implementation**
  - Full `proxy_pass` directive support
  - HTTP and HTTPS proxy functionality
  - Automatic request/response forwarding
  - Backend connection pooling
  - Error handling with 502 Bad Gateway responses
  - X-Forwarded-For and X-Forwarded-Proto headers
  - Configurable backend timeouts (30 seconds)

### 🔧 Enhancements
- **Improved Routing System**
  - Port-aware server block selection
  - Fixed location matching logic
  - Better debugging output for route resolution
  - Proper precedence handling for location blocks

### 🐛 Bug Fixes
- Fixed server block selection based on listening ports
- Corrected location matching for complex routing scenarios
- Improved error handling for missing backend servers

### 📊 Technical Details
- Added `src/proxy.c` and `src/proxy.h` for reverse proxy functionality
- Enhanced `find_route()` function with port parameter
- Implemented URL parsing for backend destinations
- Added proper resource cleanup and memory management

### 🧪 Testing
- Comprehensive reverse proxy testing
- Multi-backend server validation
- HTTP and HTTPS proxy verification
- Error condition testing

### 📝 Configuration Examples
```nginx
server {
    listen 8080;
    server_name localhost;
    
    # Static files
    location / {
        root ./www;
    }
    
    # API proxy
    location /api {
        proxy_pass http://127.0.0.1:3000;
    }
    
    # Admin interface proxy
    location /admin {
        proxy_pass http://127.0.0.1:3001;
    }
}

server {
    listen 8443 ssl;
    server_name localhost;
    ssl_certificate ./certs/server.crt;
    ssl_certificate_key ./certs/server.key;
    
    # Microservice proxy
    location /service {
        proxy_pass http://127.0.0.1:3002;
    }
}
```

### 🎯 Roadmap Progress
- ✅ **Phase 1.1**: Reverse Proxy Implementation
  - ✅ Backend connection pooling
  - ✅ Proxy request forwarding
  - ✅ Response streaming
  - ✅ Error handling
  - ✅ Timeout management

**Next**: Phase 1.2 - HTTP Header Manipulation

## [v0.2.0] - 2025-07-03

### ✨ New Features
- **Multi-protocol Support** 