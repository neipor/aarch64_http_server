# ANX HTTP Server v1.1.0+ API 参考文档

## 概述

ANX HTTP Server 是一个基于C语言和Rust混合架构的高性能HTTP服务器。本文档详细说明了Rust模块的API接口和C语言FFI接口的使用方法。

## 架构设计

### 模块结构
- **HTTP解析器模块**: 提供高性能的HTTP请求/响应解析
- **缓存模块**: 支持多种缓存策略的内存缓存系统
- **配置模块**: 支持TOML和Nginx兼容的配置解析
- **CLI模块**: 命令行参数解析和配置生成
- **FFI接口**: C语言调用Rust模块的接口层

### 设计原则
- **类型安全**: Rust模块提供编译时类型检查
- **内存安全**: 自动内存管理，避免内存泄漏
- **高性能**: 零拷贝解析，高效缓存算法
- **易用性**: 简洁的API设计，丰富的文档

## Rust模块API

### HTTP解析器模块

#### HttpRequest
```rust
pub struct HttpRequest {
    pub method: String,
    pub uri: String,
    pub version: String,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
}

impl HttpRequest {
    /// 解析HTTP请求
    pub fn parse(data: &[u8]) -> Result<Self, HttpParseError>
    
    /// 获取请求头
    pub fn get_header(&self, name: &str) -> Option<&String>
    
    /// 检查是否为keep-alive连接
    pub fn is_keep_alive(&self) -> bool
}
```

#### HttpResponse
```rust
pub struct HttpResponse {
    pub status_code: u16,
    pub reason_phrase: String,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
}

impl HttpResponse {
    /// 创建新的HTTP响应
    pub fn new(status_code: u16, reason_phrase: &str) -> Self
    
    /// 设置响应头
    pub fn set_header(&mut self, name: &str, value: &str)
    
    /// 设置响应体
    pub fn set_body(&mut self, data: &[u8])
    
    /// 转换为字节数组
    pub fn to_bytes(&self) -> Vec<u8>
}
```

### 缓存模块

#### Cache
```rust
pub struct Cache {
    // 内部实现
}

impl Cache {
    /// 创建新缓存
    pub fn new() -> Self
    
    /// 使用自定义配置创建缓存
    pub fn with_config(config: CacheConfig) -> Self
    
    /// 获取缓存值
    pub fn get(&self, key: &str) -> Option<CacheResponse>
    
    /// 条件获取缓存值
    pub fn get_conditional(&self, key: &str, if_none_match: Option<&str>, 
                          if_modified_since: Option<u64>) -> Option<CacheResponse>
    
    /// 存储缓存值
    pub fn put(&self, key: &str, data: &[u8], content_type: &str) -> Result<(), CacheError>
    
    /// 带元数据存储缓存值
    pub fn put_with_metadata(&self, key: &str, data: &[u8], content_type: &str,
                            etag: Option<&str>, last_modified: Option<u64>, 
                            ttl: Option<u64>) -> Result<(), CacheError>
    
    /// 删除缓存值
    pub fn remove(&self, key: &str) -> Result<(), CacheError>
    
    /// 清空缓存
    pub fn clear(&self)
    
    /// 获取缓存统计
    pub fn get_stats(&self) -> CacheStats
    
    /// 清理过期条目
    pub fn cleanup_expired(&self)
}
```

#### CacheConfig
```rust
pub struct CacheConfig {
    pub max_size: usize,           // 最大缓存大小（字节）
    pub max_entries: usize,        // 最大条目数
    pub default_ttl: u64,          // 默认TTL（秒）
    pub strategy: CacheStrategy,    // 缓存策略
    pub min_file_size: usize,      // 最小文件大小
    pub cacheable_types: Vec<String>, // 可缓存类型
}
```

#### CacheStrategy
```rust
pub enum CacheStrategy {
    LRU,   // 最近最少使用
    LFU,   // 最少使用频率
    FIFO,  // 先进先出
}
```

### 配置模块

#### AnxConfig
```rust
pub struct AnxConfig {
    pub server: ServerConfig,
    pub locations: Vec<LocationConfig>,
    pub logging: LoggingConfig,
    pub ssl: Option<SslConfig>,
}

impl AnxConfig {
    /// 从TOML文件加载配置
    pub fn from_toml(path: &str) -> Result<Self, ConfigError>
    
    /// 从Nginx配置文件加载配置
    pub fn from_nginx(path: &str) -> Result<Self, ConfigError>
    
    /// 验证配置
    pub fn validate(&self) -> Result<(), ConfigError>
}
```

### CLI模块

#### CliConfig
```rust
pub struct CliConfig {
    pub port: u16,
    pub host: String,
    pub static_dir: Option<String>,
    pub proxies: Vec<ProxyConfig>,
    pub ssl: Option<SslConfig>,
    pub logging: LoggingConfig,
    pub cache: CacheConfig,
    pub threads: usize,
    pub max_connections: usize,
    pub daemon: bool,
    pub pid_file: Option<String>,
}
```

#### CliParser
```rust
pub struct CliParser {
    // 内部实现
}

impl CliParser {
    /// 创建CLI解析器
    pub fn new() -> Self
    
    /// 解析命令行参数
    pub fn parse_args(&self) -> Result<CliConfig, CliError>
    
    /// 生成帮助信息
    pub fn print_help(&self)
}
```

## C语言FFI接口

### 配置接口

```c
// 加载TOML配置
ConfigHandle* anx_config_load_toml(const char *path);

// 加载Nginx配置
ConfigHandle* anx_config_load_nginx(const char *path);

// 获取配置项
char* anx_config_get_listen(const ConfigHandle *handle, size_t index);
char* anx_config_get_root(const ConfigHandle *handle);
int anx_config_get_worker_processes(const ConfigHandle *handle);
int anx_config_get_worker_connections(const ConfigHandle *handle);

// 验证配置
int anx_config_validate(const ConfigHandle *handle);

// 释放配置句柄
void anx_config_free(ConfigHandle *handle);
```

### HTTP解析器接口

```c
// 解析HTTP请求
HttpRequestHandle* anx_http_parse_request(const uint8_t *data, size_t len);

// 获取请求信息
char* anx_http_get_method(const HttpRequestHandle *handle);
char* anx_http_get_uri(const HttpRequestHandle *handle);
char* anx_http_get_version(const HttpRequestHandle *handle);
char* anx_http_get_header(const HttpRequestHandle *handle, const char *name);
uint8_t* anx_http_get_body(const HttpRequestHandle *handle, size_t *len);
int anx_http_is_keep_alive(const HttpRequestHandle *handle);

// 创建HTTP响应
HttpResponseHandle* anx_http_response_new(unsigned int status_code, const char *reason_phrase);
int anx_http_response_set_header(HttpResponseHandle *handle, const char *name, const char *value);
int anx_http_response_set_body(HttpResponseHandle *handle, const uint8_t *data, size_t len);
uint8_t* anx_http_response_to_bytes(const HttpResponseHandle *handle, size_t *len);

// 释放句柄
void anx_http_request_free(HttpRequestHandle *handle);
void anx_http_response_free(HttpResponseHandle *handle);
```

### 缓存接口

```c
// 创建缓存
CacheHandle* anx_cache_new(void);
CacheHandle* anx_cache_new_with_config(size_t max_size, size_t max_entries, 
                                      unsigned long default_ttl_secs, int strategy);

// 缓存操作
CacheResponseC* anx_cache_get(const CacheHandle *handle, const char *key);
CacheResponseC* anx_cache_get_conditional(const CacheHandle *handle, const char *key, 
                                         const char *if_none_match, unsigned long if_modified_since);
int anx_cache_put(const CacheHandle *handle, const char *key, 
                  const uint8_t *data, size_t data_len, const char *content_type);
int anx_cache_put_with_metadata(const CacheHandle *handle, const char *key, 
                                const uint8_t *data, size_t data_len, 
                                const char *content_type, const char *etag, 
                                unsigned long last_modified, unsigned long ttl_secs);
int anx_cache_remove(const CacheHandle *handle, const char *key);
void anx_cache_clear(const CacheHandle *handle);

// 缓存统计
CacheStatsC* anx_cache_get_stats(const CacheHandle *handle);
void anx_cache_cleanup_expired(const CacheHandle *handle);
char* anx_cache_generate_etag(const uint8_t *data, size_t len, unsigned long last_modified);

// 释放缓存资源
void anx_cache_free(CacheHandle *handle);
void anx_cache_response_free(CacheResponseC *response);
void anx_cache_stats_free(CacheStatsC *stats);
```

### CLI接口

```c
// CLI解析器
CliParser* anx_cli_parser_create(void);
CliConfigHandle* anx_cli_parse_args(CliParser *parser);
void anx_cli_parser_free(CliParser *parser);

// 获取CLI配置
uint16_t anx_cli_get_port(const CliConfigHandle *handle);
char* anx_cli_get_host(const CliConfigHandle *handle);
char* anx_cli_get_static_dir(const CliConfigHandle *handle);
size_t anx_cli_get_proxy_count(const CliConfigHandle *handle);
char* anx_cli_get_proxy_url(const CliConfigHandle *handle, size_t index);
char* anx_cli_get_proxy_path_prefix(const CliConfigHandle *handle, size_t index);
int anx_cli_is_ssl_enabled(const CliConfigHandle *handle);
char* anx_cli_get_ssl_cert_file(const CliConfigHandle *handle);
char* anx_cli_get_ssl_key_file(const CliConfigHandle *handle);
char* anx_cli_get_log_level(const CliConfigHandle *handle);
char* anx_cli_get_log_file(const CliConfigHandle *handle);
int anx_cli_is_cache_enabled(const CliConfigHandle *handle);
size_t anx_cli_get_cache_size(const CliConfigHandle *handle);
unsigned long anx_cli_get_cache_ttl(const CliConfigHandle *handle);
size_t anx_cli_get_threads(const CliConfigHandle *handle);
size_t anx_cli_get_max_connections(const CliConfigHandle *handle);
int anx_cli_is_daemon(const CliConfigHandle *handle);
char* anx_cli_get_pid_file(const CliConfigHandle *handle);

// 释放CLI配置
void anx_cli_config_free(CliConfigHandle *handle);
```

### 工具函数

```c
// 释放字符串
void anx_free_string(char *ptr);

// 释放字节数组
void anx_free_bytes(uint8_t *ptr, size_t len);
```

## 使用示例

### Rust模块使用示例

```rust
use anx_core::http_parser::{HttpRequest, HttpResponse};
use anx_core::cache::{Cache, CacheConfig, CacheStrategy};
use anx_core::config::AnxConfig;

// HTTP解析示例
let request_data = b"GET /api/users HTTP/1.1\r\nHost: example.com\r\n\r\n";
let request = HttpRequest::parse(request_data)?;
println!("Method: {}", request.method);
println!("URI: {}", request.uri);

// 缓存示例
let config = CacheConfig {
    max_size: 1024 * 1024,  // 1MB
    max_entries: 1000,
    default_ttl: 3600,       // 1小时
    strategy: CacheStrategy::LRU,
    min_file_size: 1024,
    cacheable_types: vec!["text/html".to_string(), "application/json".to_string()],
};

let cache = Cache::with_config(config);
cache.put("key1", b"Hello, World!", "text/plain")?;

if let Some(response) = cache.get("key1") {
    println!("Cached data: {:?}", response.data);
}

// 配置示例
let config = AnxConfig::from_toml("config.toml")?;
config.validate()?;
```

### C语言FFI使用示例

```c
#include "src/include/anx_rust.h"
#include <stdio.h>
#include <string.h>

int main() {
    // HTTP解析示例
    const char* request_data = 
        "GET /api/users HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    
    HttpRequestHandle* req = anx_http_parse_request(
        (const uint8_t*)request_data, 
        strlen(request_data)
    );
    
    if (req) {
        char* method = anx_http_get_method(req);
        char* uri = anx_http_get_uri(req);
        printf("Method: %s\n", method);
        printf("URI: %s\n", uri);
        
        anx_free_string(method);
        anx_free_string(uri);
        anx_http_request_free(req);
    }
    
    // 缓存示例
    CacheHandle* cache = anx_cache_new_with_config(
        1024 * 1024,  // 1MB
        1000,          // 1000 entries
        3600,          // 1 hour TTL
        0              // LRU strategy
    );
    
    const char* key = "test_key";
    const char* data = "Hello, World!";
    anx_cache_put(cache, key, (const uint8_t*)data, strlen(data), "text/plain");
    
    CacheResponseC* response = anx_cache_get(cache, key);
    if (response) {
        printf("Cached data: %.*s\n", (int)response->data_len, response->data);
        anx_cache_response_free(response);
    }
    
    anx_cache_free(cache);
    
    return 0;
}
```

## 错误处理

### Rust错误类型

```rust
#[derive(Debug, thiserror::Error)]
pub enum HttpParseError {
    #[error("Invalid HTTP format")]
    InvalidFormat,
    #[error("Incomplete request")]
    Incomplete,
    #[error("Unsupported method: {0}")]
    UnsupportedMethod(String),
}

#[derive(Debug, thiserror::Error)]
pub enum CacheError {
    #[error("Key not found: {0}")]
    KeyNotFound(String),
    #[error("Cache full")]
    CacheFull,
    #[error("Invalid data")]
    InvalidData,
}

#[derive(Debug, thiserror::Error)]
pub enum ConfigError {
    #[error("File not found: {0}")]
    FileNotFound(String),
    #[error("Parse error: {0}")]
    ParseError(String),
    #[error("Validation error: {0}")]
    ValidationError(String),
}
```

### C语言错误处理

C语言接口通过返回值表示错误：
- 返回指针的函数：`NULL`表示错误
- 返回整数的函数：`0`表示成功，非零值表示错误
- 返回布尔值的函数：`0`表示false，`1`表示true

## 性能优化

### 缓存策略选择

1. **LRU (Least Recently Used)**: 适合访问模式相对均匀的场景
2. **LFU (Least Frequently Used)**: 适合热点数据明显的场景
3. **FIFO (First In First Out)**: 适合数据访问频率相近的场景

### 内存管理

- Rust模块自动管理内存，无需手动释放
- C语言FFI接口需要手动释放分配的资源
- 使用`anx_free_string()`和`anx_free_bytes()`释放字符串和字节数组

### 线程安全

- 所有Rust模块都是线程安全的
- 缓存模块使用内部锁机制保证并发安全
- HTTP解析器是无状态的，可以安全地在多线程中使用

## 最佳实践

1. **错误处理**: 始终检查FFI函数的返回值
2. **资源管理**: 及时释放分配的资源，避免内存泄漏
3. **配置验证**: 在启动服务器前验证配置文件的正确性
4. **缓存调优**: 根据实际使用情况调整缓存大小和策略
5. **性能监控**: 定期检查缓存命中率和响应时间

## 版本兼容性

- v1.1.0+: 新增CLI模块和命令行参数支持
- v1.1.0: 新增缓存模块和HTTP解析器
- v1.0.0: 基础HTTP服务器功能

## 技术支持

如有问题，请参考：
- 项目文档: `docs/`
- 示例代码: `examples/`
- 测试用例: `tests/`
- 问题反馈: GitHub Issues 