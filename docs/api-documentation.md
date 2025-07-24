# ANX HTTP Server API 文档

## 目录
1. [简介](#简介)
2. [C语言API](#c语言api)
3. [Rust FFI API](#rust-ffi-api)
4. [配置API](#配置api)
5. [HTTP解析API](#http解析api)
6. [缓存API](#缓存api)
7. [CLI API](#cli-api)
8. [工具函数](#工具函数)

## 简介

ANX HTTP Server 提供了丰富的API接口，支持C语言原生API和Rust FFI接口。这些API允许开发者灵活地集成和扩展ANX的功能。

## C语言API

### 主要API函数

#### `anx_init`
初始化ANX服务器
```c
anx_int_t anx_init(const char *config_file);
```
- **参数**: `config_file` - 配置文件路径
- **返回值**: 成功返回ANX_OK，失败返回错误码

#### `anx_start`
启动ANX服务器
```c
anx_int_t anx_start(void);
```
- **返回值**: 成功返回ANX_OK，失败返回错误码

#### `anx_stop`
停止ANX服务器
```c
anx_int_t anx_stop(void);
```
- **返回值**: 成功返回ANX_OK，失败返回错误码

#### `anx_reload`
重载ANX服务器配置
```c
anx_int_t anx_reload(const char *config_file);
```
- **参数**: `config_file` - 新的配置文件路径
- **返回值**: 成功返回ANX_OK，失败返回错误码

#### `anx_cleanup`
清理ANX服务器资源
```c
void anx_cleanup(void);
```

#### `anx_get_status`
获取服务器状态信息
```c
const char* anx_get_status(void);
```
- **返回值**: 状态信息字符串

## Rust FFI API

### 配置API

#### `anx_config_load_toml`
从TOML文件加载配置
```c
ConfigHandle* anx_config_load_toml(const char *path);
```
- **参数**: `path` - 配置文件路径
- **返回值**: 配置句柄，失败时返回NULL

#### `anx_config_load_nginx`
从Nginx文件加载配置
```c
ConfigHandle* anx_config_load_nginx(const char *path);
```
- **参数**: `path` - 配置文件路径
- **返回值**: 配置句柄，失败时返回NULL

#### `anx_config_get_listen`
获取服务器监听地址
```c
char* anx_config_get_listen(const ConfigHandle *handle, size_t index);
```
- **参数**: 
  - `handle` - 配置句柄
  - `index` - 监听地址索引
- **返回值**: 监听地址字符串，需要调用`anx_free_string`释放

#### `anx_config_get_root`
获取服务器根目录
```c
char* anx_config_get_root(const ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄
- **返回值**: 根目录路径字符串，需要调用`anx_free_string`释放

#### `anx_config_get_worker_processes`
获取工作进程数
```c
int anx_config_get_worker_processes(const ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄
- **返回值**: 工作进程数

#### `anx_config_get_worker_connections`
获取工作连接数
```c
int anx_config_get_worker_connections(const ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄
- **返回值**: 工作连接数

#### `anx_config_get_locations_count`
获取位置配置数量
```c
int anx_config_get_locations_count(const ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄
- **返回值**: 位置配置数量

#### `anx_config_get_location_path`
获取位置路径
```c
char* anx_config_get_location_path(const ConfigHandle *handle, size_t index);
```
- **参数**: 
  - `handle` - 配置句柄
  - `index` - 位置索引
- **返回值**: 位置路径字符串，需要调用`anx_free_string`释放

#### `anx_config_validate`
验证配置
```c
int anx_config_validate(const ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄
- **返回值**: 验证成功返回0，失败返回非0值

#### `anx_config_free`
释放配置句柄
```c
void anx_config_free(ConfigHandle *handle);
```
- **参数**: `handle` - 配置句柄

### HTTP解析API

#### `anx_http_parse_request`
解析HTTP请求
```c
HttpRequestHandle* anx_http_parse_request(const uint8_t *data, size_t len);
```
- **参数**: 
  - `data` - HTTP请求数据
  - `len` - 数据长度
- **返回值**: HTTP请求句柄，失败时返回NULL

#### `anx_http_get_method`
获取HTTP请求方法
```c
char* anx_http_get_method(const HttpRequestHandle *handle);
```
- **参数**: `handle` - HTTP请求句柄
- **返回值**: HTTP方法字符串，需要调用`anx_free_string`释放

#### `anx_http_get_uri`
获取HTTP请求URI
```c
char* anx_http_get_uri(const HttpRequestHandle *handle);
```
- **参数**: `handle` - HTTP请求句柄
- **返回值**: URI字符串，需要调用`anx_free_string`释放

#### `anx_http_get_version`
获取HTTP请求版本
```c
char* anx_http_get_version(const HttpRequestHandle *handle);
```
- **参数**: `handle` - HTTP请求句柄
- **返回值**: HTTP版本字符串，需要调用`anx_free_string`释放

#### `anx_http_get_header`
获取HTTP请求头
```c
char* anx_http_get_header(const HttpRequestHandle *handle, const char *name);
```
- **参数**: 
  - `handle` - HTTP请求句柄
  - `name` - 头名称
- **返回值**: 头值字符串，需要调用`anx_free_string`释放

#### `anx_http_get_body`
获取HTTP请求体
```c
uint8_t* anx_http_get_body(const HttpRequestHandle *handle, size_t *len);
```
- **参数**: 
  - `handle` - HTTP请求句柄
  - `len` - 输出参数，返回数据长度
- **返回值**: 请求体数据，需要调用`anx_free_bytes`释放

#### `anx_http_is_keep_alive`
检查是否为keep-alive请求
```c
int anx_http_is_keep_alive(const HttpRequestHandle *handle);
```
- **参数**: `handle` - HTTP请求句柄
- **返回值**: 是keep-alive请求返回1，否则返回0

#### `anx_http_request_free`
释放HTTP请求句柄
```c
void anx_http_request_free(HttpRequestHandle *handle);
```
- **参数**: `handle` - HTTP请求句柄

#### `anx_http_response_new`
创建HTTP响应
```c
HttpResponseHandle* anx_http_response_new(unsigned int status_code, const char *reason_phrase);
```
- **参数**: 
  - `status_code` - 状态码
  - `reason_phrase` - 状态描述
- **返回值**: HTTP响应句柄

#### `anx_http_response_set_header`
设置HTTP响应头
```c
int anx_http_response_set_header(HttpResponseHandle *handle, const char *name, const char *value);
```
- **参数**: 
  - `handle` - HTTP响应句柄
  - `name` - 头名称
  - `value` - 头值
- **返回值**: 成功返回0，失败返回非0值

#### `anx_http_response_set_body`
设置HTTP响应体
```c
int anx_http_response_set_body(HttpResponseHandle *handle, const uint8_t *data, size_t len);
```
- **参数**: 
  - `handle` - HTTP响应句柄
  - `data` - 响应体数据
  - `len` - 数据长度
- **返回值**: 成功返回0，失败返回非0值

#### `anx_http_response_to_bytes`
将HTTP响应转换为字节
```c
uint8_t* anx_http_response_to_bytes(const HttpResponseHandle *handle, size_t *len);
```
- **参数**: 
  - `handle` - HTTP响应句柄
  - `len` - 输出参数，返回数据长度
- **返回值**: 响应数据，需要调用`anx_free_bytes`释放

#### `anx_http_response_free`
释放HTTP响应句柄
```c
void anx_http_response_free(HttpResponseHandle *handle);
```
- **参数**: `handle` - HTTP响应句柄

### 缓存API

#### `anx_cache_new`
创建默认缓存
```c
CacheHandle* anx_cache_new(void);
```
- **返回值**: 缓存句柄

#### `anx_cache_new_with_config`
创建自定义配置缓存
```c
CacheHandle* anx_cache_new_with_config(size_t max_size, size_t max_entries, 
                                      unsigned long default_ttl_secs, int strategy);
```
- **参数**: 
  - `max_size` - 最大大小
  - `max_entries` - 最大条目数
  - `default_ttl_secs` - 默认TTL（秒）
  - `strategy` - 缓存策略（ANX_CACHE_STRATEGY_LRU, ANX_CACHE_STRATEGY_LFU, ANX_CACHE_STRATEGY_FIFO）
- **返回值**: 缓存句柄

#### `anx_cache_get`
从缓存获取值
```c
CacheResponseC* anx_cache_get(const CacheHandle *handle, const char *key);
```
- **参数**: 
  - `handle` - 缓存句柄
  - `key` - 键
- **返回值**: 缓存响应，需要调用`anx_cache_response_free`释放

#### `anx_cache_get_conditional`
带条件头从缓存获取值
```c
CacheResponseC* anx_cache_get_conditional(const CacheHandle *handle, const char *key, 
                                         const char *if_none_match, unsigned long if_modified_since);
```
- **参数**: 
  - `handle` - 缓存句柄
  - `key` - 键
  - `if_none_match` - If-None-Match头值
  - `if_modified_since` - If-Modified-Since头值
- **返回值**: 缓存响应，需要调用`anx_cache_response_free`释放

#### `anx_cache_put`
向缓存添加值
```c
int anx_cache_put(const CacheHandle *handle, const char *key, 
                  const uint8_t *data, size_t data_len, const char *content_type);
```
- **参数**: 
  - `handle` - 缓存句柄
  - `key` - 键
  - `data` - 数据
  - `data_len` - 数据长度
  - `content_type` - 内容类型
- **返回值**: 成功返回0，失败返回非0值

#### `anx_cache_put_with_metadata`
带元数据向缓存添加值
```c
int anx_cache_put_with_metadata(const CacheHandle *handle, const char *key, 
                                const uint8_t *data, size_t data_len, 
                                const char *content_type, const char *etag, 
                                unsigned long last_modified, unsigned long ttl_secs);
```
- **参数**: 
  - `handle` - 缓存句柄
  - `key` - 键
  - `data` - 数据
  - `data_len` - 数据长度
  - `content_type` - 内容类型
  - `etag` - ETag
  - `last_modified` - 最后修改时间
  - `ttl_secs` - TTL（秒）
- **返回值**: 成功返回0，失败返回非0值

#### `anx_cache_remove`
从缓存删除值
```c
int anx_cache_remove(const CacheHandle *handle, const char *key);
```
- **参数**: 
  - `handle` - 缓存句柄
  - `key` - 键
- **返回值**: 成功返回0，失败返回非0值

#### `anx_cache_clear`
清空缓存
```c
void anx_cache_clear(const CacheHandle *handle);
```
- **参数**: `handle` - 缓存句柄

#### `anx_cache_get_stats`
获取缓存统计信息
```c
CacheStatsC* anx_cache_get_stats(const CacheHandle *handle);
```
- **参数**: `handle` - 缓存句柄
- **返回值**: 缓存统计信息，需要调用`anx_cache_stats_free`释放

#### `anx_cache_cleanup_expired`
清理过期条目
```c
void anx_cache_cleanup_expired(const CacheHandle *handle);
```
- **参数**: `handle` - 缓存句柄

#### `anx_cache_generate_etag`
为内容生成ETag
```c
char* anx_cache_generate_etag(const uint8_t *data, size_t len, unsigned long last_modified);
```
- **参数**: 
  - `data` - 数据
  - `len` - 数据长度
  - `last_modified` - 最后修改时间
- **返回值**: ETag字符串，需要调用`anx_free_string`释放

#### `anx_cache_free`
释放缓存句柄
```c
void anx_cache_free(CacheHandle *handle);
```
- **参数**: `handle` - 缓存句柄

#### `anx_cache_response_free`
释放缓存响应
```c
void anx_cache_response_free(CacheResponseC *response);
```
- **参数**: `response` - 缓存响应

#### `anx_cache_stats_free`
释放缓存统计信息
```c
void anx_cache_stats_free(CacheStatsC *stats);
```
- **参数**: `stats` - 缓存统计信息

### CLI API

#### `anx_cli_parser_create`
创建CLI解析器
```c
CliParser* anx_cli_parser_create(void);
```
- **返回值**: CLI解析器句柄

#### `anx_cli_parse_args`
解析命令行参数
```c
CliConfigHandle* anx_cli_parse_args(CliParser *parser);
```
- **参数**: `parser` - CLI解析器句柄
- **返回值**: CLI配置句柄

#### `anx_cli_get_port`
获取端口
```c
uint16_t anx_cli_get_port(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 端口号

#### `anx_cli_get_host`
获取主机
```c
char* anx_cli_get_host(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 主机字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_static_dir`
获取静态目录
```c
char* anx_cli_get_static_dir(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 静态目录字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_proxy_count`
获取代理数量
```c
size_t anx_cli_get_proxy_count(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 代理数量

#### `anx_cli_get_proxy_url`
获取代理URL
```c
char* anx_cli_get_proxy_url(const CliConfigHandle *handle, size_t index);
```
- **参数**: 
  - `handle` - CLI配置句柄
  - `index` - 代理索引
- **返回值**: 代理URL字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_proxy_path_prefix`
获取代理路径前缀
```c
char* anx_cli_get_proxy_path_prefix(const CliConfigHandle *handle, size_t index);
```
- **参数**: 
  - `handle` - CLI配置句柄
  - `index` - 代理索引
- **返回值**: 代理路径前缀字符串，需要调用`anx_free_string`释放

#### `anx_cli_is_ssl_enabled`
检查SSL是否启用
```c
int anx_cli_is_ssl_enabled(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 启用返回1，否则返回0

#### `anx_cli_get_ssl_cert_file`
获取SSL证书文件
```c
char* anx_cli_get_ssl_cert_file(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: SSL证书文件路径字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_ssl_key_file`
获取SSL密钥文件
```c
char* anx_cli_get_ssl_key_file(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: SSL密钥文件路径字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_log_level`
获取日志级别
```c
char* anx_cli_get_log_level(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 日志级别字符串，需要调用`anx_free_string`释放

#### `anx_cli_get_log_file`
获取日志文件
```c
char* anx_cli_get_log_file(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 日志文件路径字符串，需要调用`anx_free_string`释放

#### `anx_cli_is_cache_enabled`
检查缓存是否启用
```c
int anx_cli_is_cache_enabled(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 启用返回1，否则返回0

#### `anx_cli_get_cache_size`
获取缓存大小
```c
size_t anx_cli_get_cache_size(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 缓存大小（字节）

#### `anx_cli_get_cache_ttl`
获取缓存TTL
```c
unsigned long anx_cli_get_cache_ttl(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 缓存TTL（秒）

#### `anx_cli_get_threads`
获取线程数
```c
size_t anx_cli_get_threads(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 线程数

#### `anx_cli_get_max_connections`
获取最大连接数
```c
size_t anx_cli_get_max_connections(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 最大连接数

#### `anx_cli_is_daemon`
检查是否为守护进程模式
```c
int anx_cli_is_daemon(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: 守护进程模式返回1，否则返回0

#### `anx_cli_get_pid_file`
获取PID文件
```c
char* anx_cli_get_pid_file(const CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄
- **返回值**: PID文件路径字符串，需要调用`anx_free_string`释放

#### `anx_cli_config_free`
释放CLI配置句柄
```c
void anx_cli_config_free(CliConfigHandle *handle);
```
- **参数**: `handle` - CLI配置句柄

#### `anx_cli_parser_free`
释放CLI解析器
```c
void anx_cli_parser_free(CliParser *parser);
```
- **参数**: `parser` - CLI解析器句柄

### 工具函数

#### `anx_free_string`
释放Rust分配的C字符串
```c
void anx_free_string(char *ptr);
```
- **参数**: `ptr` - 字符串指针

#### `anx_free_bytes`
释放Rust分配的字节数组
```c
void anx_free_bytes(uint8_t *ptr, size_t len);
```
- **参数**: 
  - `ptr` - 字节数组指针
  - `len` - 数组长度

#### `anx_rust_init`
初始化Rust模块
```c
int anx_rust_init(void);
```
- **返回值**: 成功返回0，失败返回非0值

#### `anx_rust_cleanup`
清理Rust模块
```c
void anx_rust_cleanup(void);