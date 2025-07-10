//! FFI (Foreign Function Interface) module for ANX HTTP Server
//! 
//! This module provides C-compatible interfaces for Rust functionality.

use std::ffi::{CStr, CString};
use std::os::raw::{c_char, c_int, c_uint, c_ulong};
use std::ptr;
use std::time::{SystemTime, UNIX_EPOCH};

use super::config::{AnxConfig, ConfigError};
use super::http_parser::{HttpRequest, HttpResponse, HttpParseError, HttpMethod};
use super::cache::{Cache, CacheConfig, CacheResponse, CacheError, CacheStrategy};

/// C-compatible configuration handle
pub struct ConfigHandle {
    config: Box<AnxConfig>,
}

/// C-compatible HTTP request handle
pub struct HttpRequestHandle {
    request: Box<HttpRequest>,
}

/// C-compatible HTTP response handle
pub struct HttpResponseHandle {
    response: Box<HttpResponse>,
}

/// C-compatible cache handle
pub struct CacheHandle {
    cache: Box<Cache>,
}

/// C-compatible cache response
#[repr(C)]
pub struct CacheResponseC {
    pub data: *mut u8,
    pub data_len: usize,
    pub content_type: *mut c_char,
    pub etag: *mut c_char,
    pub last_modified: c_ulong,
    pub is_compressed: c_int,
    pub needs_validation: c_int,
}

/// C-compatible cache statistics
#[repr(C)]
pub struct CacheStatsC {
    pub hits: c_ulong,
    pub misses: c_ulong,
    pub puts: c_ulong,
    pub evictions: c_ulong,
    pub current_size: usize,
    pub current_entries: usize,
    pub hit_rate: f64,
}

// =============================================================================
// Configuration Functions
// =============================================================================

/// Load configuration from TOML file
#[no_mangle]
pub extern "C" fn anx_config_load_toml(path: *const c_char) -> *mut ConfigHandle {
    if path.is_null() {
        return ptr::null_mut();
    }
    
    let path_str = unsafe {
        match CStr::from_ptr(path).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    match AnxConfig::from_toml_file(path_str) {
        Ok(config) => {
            let handle = Box::new(ConfigHandle {
                config: Box::new(config),
            });
            Box::into_raw(handle)
        }
        Err(_) => ptr::null_mut(),
    }
}

/// Load configuration from Nginx file
#[no_mangle]
pub extern "C" fn anx_config_load_nginx(path: *const c_char) -> *mut ConfigHandle {
    if path.is_null() {
        return ptr::null_mut();
    }
    
    let path_str = unsafe {
        match CStr::from_ptr(path).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    match AnxConfig::from_nginx_file(path_str) {
        Ok(config) => {
            let handle = Box::new(ConfigHandle {
                config: Box::new(config),
            });
            Box::into_raw(handle)
        }
        Err(_) => ptr::null_mut(),
    }
}

/// Get server listen addresses
#[no_mangle]
pub extern "C" fn anx_config_get_listen(handle: *const ConfigHandle, index: usize) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    if index >= handle.config.server.listen.len() {
        return ptr::null_mut();
    }
    
    match CString::new(handle.config.server.listen[index].clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Get server root directory
#[no_mangle]
pub extern "C" fn anx_config_get_root(handle: *const ConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    match CString::new(handle.config.server.root.clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Get server worker processes count
#[no_mangle]
pub extern "C" fn anx_config_get_worker_processes(handle: *const ConfigHandle) -> c_int {
    if handle.is_null() {
        return 1;
    }
    
    let handle = unsafe { &*handle };
    handle.config.server.worker_processes.unwrap_or(1) as c_int
}

/// Get server worker connections count
#[no_mangle]
pub extern "C" fn anx_config_get_worker_connections(handle: *const ConfigHandle) -> c_int {
    if handle.is_null() {
        return 1024;
    }
    
    let handle = unsafe { &*handle };
    handle.config.server.worker_connections.unwrap_or(1024) as c_int
}

/// Get number of locations
#[no_mangle]
pub extern "C" fn anx_config_get_locations_count(handle: *const ConfigHandle) -> c_int {
    if handle.is_null() {
        return 0;
    }
    
    let handle = unsafe { &*handle };
    handle.config.locations.len() as c_int
}

/// Get location path by index
#[no_mangle]
pub extern "C" fn anx_config_get_location_path(handle: *const ConfigHandle, index: usize) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    if index >= handle.config.locations.len() {
        return ptr::null_mut();
    }
    
    match CString::new(handle.config.locations[index].path.clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Free configuration handle
#[no_mangle]
pub extern "C" fn anx_config_free(handle: *mut ConfigHandle) {
    if !handle.is_null() {
        unsafe {
            Box::from_raw(handle);
        }
    }
}

/// Validate configuration
#[no_mangle]
pub extern "C" fn anx_config_validate(handle: *const ConfigHandle) -> c_int {
    if handle.is_null() {
        return -1;
    }
    
    let handle = unsafe { &*handle };
    
    match handle.config.validate() {
        Ok(()) => 0,
        Err(_) => -1,
    }
}

// =============================================================================
// HTTP Parser Functions
// =============================================================================

/// Parse HTTP request from raw data
#[no_mangle]
pub extern "C" fn anx_http_parse_request(data: *const u8, len: usize) -> *mut HttpRequestHandle {
    if data.is_null() || len == 0 {
        return ptr::null_mut();
    }
    
    let slice = unsafe { std::slice::from_raw_parts(data, len) };
    
    match HttpRequest::parse(slice) {
        Ok(request) => {
            let handle = Box::new(HttpRequestHandle {
                request: Box::new(request),
            });
            Box::into_raw(handle)
        }
        Err(_) => ptr::null_mut(),
    }
}

/// Get HTTP request method
#[no_mangle]
pub extern "C" fn anx_http_get_method(handle: *const HttpRequestHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    match CString::new(handle.request.method.clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Get HTTP request URI
#[no_mangle]
pub extern "C" fn anx_http_get_uri(handle: *const HttpRequestHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    match CString::new(handle.request.uri.clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Get HTTP request version
#[no_mangle]
pub extern "C" fn anx_http_get_version(handle: *const HttpRequestHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    match CString::new(handle.request.version.clone()) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Get HTTP request header value
#[no_mangle]
pub extern "C" fn anx_http_get_header(handle: *const HttpRequestHandle, name: *const c_char) -> *mut c_char {
    if handle.is_null() || name.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    let name_str = unsafe {
        match CStr::from_ptr(name).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    match handle.request.get_header(name_str) {
        Some(value) => match CString::new(value.clone()) {
            Ok(c_str) => c_str.into_raw(),
            Err(_) => ptr::null_mut(),
        },
        None => ptr::null_mut(),
    }
}

/// Get HTTP request body
#[no_mangle]
pub extern "C" fn anx_http_get_body(handle: *const HttpRequestHandle, len: *mut usize) -> *mut u8 {
    if handle.is_null() || len.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    
    if handle.request.body.is_empty() {
        unsafe { *len = 0; }
        return ptr::null_mut();
    }
    
    let mut body = handle.request.body.clone();
    unsafe { *len = body.len(); }
    
    let ptr = body.as_mut_ptr();
    std::mem::forget(body);
    ptr
}

/// Check if request is keep-alive
#[no_mangle]
pub extern "C" fn anx_http_is_keep_alive(handle: *const HttpRequestHandle) -> c_int {
    if handle.is_null() {
        return 0;
    }
    
    let handle = unsafe { &*handle };
    if handle.request.is_keep_alive() { 1 } else { 0 }
}

/// Free HTTP request handle
#[no_mangle]
pub extern "C" fn anx_http_request_free(handle: *mut HttpRequestHandle) {
    if !handle.is_null() {
        unsafe {
            Box::from_raw(handle);
        }
    }
}

/// Create HTTP response
#[no_mangle]
pub extern "C" fn anx_http_response_new(status_code: c_uint, reason_phrase: *const c_char) -> *mut HttpResponseHandle {
    if reason_phrase.is_null() {
        return ptr::null_mut();
    }
    
    let reason_str = unsafe {
        match CStr::from_ptr(reason_phrase).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    let response = HttpResponse::new(status_code as u16, reason_str);
    let handle = Box::new(HttpResponseHandle {
        response: Box::new(response),
    });
    Box::into_raw(handle)
}

/// Set HTTP response header
#[no_mangle]
pub extern "C" fn anx_http_response_set_header(handle: *mut HttpResponseHandle, name: *const c_char, value: *const c_char) -> c_int {
    if handle.is_null() || name.is_null() || value.is_null() {
        return -1;
    }
    
    let handle = unsafe { &mut *handle };
    let name_str = unsafe {
        match CStr::from_ptr(name).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    let value_str = unsafe {
        match CStr::from_ptr(value).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    
    handle.response.set_header(name_str.to_string(), value_str.to_string());
    0
}

/// Set HTTP response body
#[no_mangle]
pub extern "C" fn anx_http_response_set_body(handle: *mut HttpResponseHandle, data: *const u8, len: usize) -> c_int {
    if handle.is_null() || data.is_null() {
        return -1;
    }
    
    let handle = unsafe { &mut *handle };
    let body = unsafe { std::slice::from_raw_parts(data, len) };
    
    handle.response.set_body(body.to_vec());
    0
}

/// Convert HTTP response to bytes
#[no_mangle]
pub extern "C" fn anx_http_response_to_bytes(handle: *const HttpResponseHandle, len: *mut usize) -> *mut u8 {
    if handle.is_null() || len.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    let mut bytes = handle.response.to_bytes();
    
    unsafe { *len = bytes.len(); }
    
    let ptr = bytes.as_mut_ptr();
    std::mem::forget(bytes);
    ptr
}

/// Free HTTP response handle
#[no_mangle]
pub extern "C" fn anx_http_response_free(handle: *mut HttpResponseHandle) {
    if !handle.is_null() {
        unsafe {
            Box::from_raw(handle);
        }
    }
}

// =============================================================================
// Cache Functions
// =============================================================================

/// Create new cache with default configuration
#[no_mangle]
pub extern "C" fn anx_cache_new() -> *mut CacheHandle {
    let cache = Cache::new();
    let handle = Box::new(CacheHandle {
        cache: Box::new(cache),
    });
    Box::into_raw(handle)
}

/// Create new cache with custom configuration
#[no_mangle]
pub extern "C" fn anx_cache_new_with_config(
    max_size: usize,
    max_entries: usize,
    default_ttl_secs: c_ulong,
    strategy: c_int,
) -> *mut CacheHandle {
    let cache_strategy = match strategy {
        0 => CacheStrategy::LRU,
        1 => CacheStrategy::LFU,
        2 => CacheStrategy::FIFO,
        _ => CacheStrategy::LRU,
    };
    
    let mut config = CacheConfig::default();
    config.max_size = max_size;
    config.max_entries = max_entries;
    config.default_ttl = std::time::Duration::from_secs(default_ttl_secs);
    config.strategy = cache_strategy;
    
    let cache = Cache::with_config(config);
    let handle = Box::new(CacheHandle {
        cache: Box::new(cache),
    });
    Box::into_raw(handle)
}

/// Get value from cache
#[no_mangle]
pub extern "C" fn anx_cache_get(handle: *const CacheHandle, key: *const c_char) -> *mut CacheResponseC {
    if handle.is_null() || key.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    let key_str = unsafe {
        match CStr::from_ptr(key).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    match handle.cache.get(key_str) {
        Ok(response) => {
            let mut data = response.data.clone();
            let data_ptr = data.as_mut_ptr();
            std::mem::forget(data);
            
            let content_type_ptr = response.content_type.as_ref()
                .and_then(|ct| CString::new(ct.clone()).ok())
                .map(|cs| cs.into_raw())
                .unwrap_or(ptr::null_mut());
            
            let etag_ptr = response.etag.as_ref()
                .and_then(|et| CString::new(et.clone()).ok())
                .map(|cs| cs.into_raw())
                .unwrap_or(ptr::null_mut());
            
            let last_modified = response.last_modified
                .and_then(|lm| lm.duration_since(UNIX_EPOCH).ok())
                .map(|d| d.as_secs())
                .unwrap_or(0);
            
            let cache_response = Box::new(CacheResponseC {
                data: data_ptr,
                data_len: response.data.len(),
                content_type: content_type_ptr,
                etag: etag_ptr,
                last_modified: last_modified as c_ulong,
                is_compressed: if response.is_compressed { 1 } else { 0 },
                needs_validation: if response.needs_validation { 1 } else { 0 },
            });
            
            Box::into_raw(cache_response)
        }
        Err(_) => ptr::null_mut(),
    }
}

/// Get value from cache with conditional headers
#[no_mangle]
pub extern "C" fn anx_cache_get_conditional(
    handle: *const CacheHandle,
    key: *const c_char,
    if_none_match: *const c_char,
    if_modified_since: c_ulong,
) -> *mut CacheResponseC {
    if handle.is_null() || key.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    let key_str = unsafe {
        match CStr::from_ptr(key).to_str() {
            Ok(s) => s,
            Err(_) => return ptr::null_mut(),
        }
    };
    
    let etag_opt = if if_none_match.is_null() {
        None
    } else {
        unsafe {
            CStr::from_ptr(if_none_match).to_str().ok()
        }
    };
    
    let modified_since_opt = if if_modified_since == 0 {
        None
    } else {
        Some(UNIX_EPOCH + std::time::Duration::from_secs(if_modified_since))
    };
    
    match handle.cache.get_conditional(key_str, etag_opt, modified_since_opt) {
        Ok(response) => {
            let mut data = response.data.clone();
            let data_ptr = data.as_mut_ptr();
            std::mem::forget(data);
            
            let content_type_ptr = response.content_type.as_ref()
                .and_then(|ct| CString::new(ct.clone()).ok())
                .map(|cs| cs.into_raw())
                .unwrap_or(ptr::null_mut());
            
            let etag_ptr = response.etag.as_ref()
                .and_then(|et| CString::new(et.clone()).ok())
                .map(|cs| cs.into_raw())
                .unwrap_or(ptr::null_mut());
            
            let last_modified = response.last_modified
                .and_then(|lm| lm.duration_since(UNIX_EPOCH).ok())
                .map(|d| d.as_secs())
                .unwrap_or(0);
            
            let cache_response = Box::new(CacheResponseC {
                data: data_ptr,
                data_len: response.data.len(),
                content_type: content_type_ptr,
                etag: etag_ptr,
                last_modified: last_modified as c_ulong,
                is_compressed: if response.is_compressed { 1 } else { 0 },
                needs_validation: if response.needs_validation { 1 } else { 0 },
            });
            
            Box::into_raw(cache_response)
        }
        Err(_) => ptr::null_mut(),
    }
}

/// Put value into cache
#[no_mangle]
pub extern "C" fn anx_cache_put(
    handle: *const CacheHandle,
    key: *const c_char,
    data: *const u8,
    data_len: usize,
    content_type: *const c_char,
) -> c_int {
    if handle.is_null() || key.is_null() || data.is_null() {
        return -1;
    }
    
    let handle = unsafe { &*handle };
    let key_str = unsafe {
        match CStr::from_ptr(key).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    
    let data_slice = unsafe { std::slice::from_raw_parts(data, data_len) };
    
    let content_type_opt = if content_type.is_null() {
        None
    } else {
        unsafe {
            CStr::from_ptr(content_type).to_str().ok().map(|s| s.to_string())
        }
    };
    
    match handle.cache.put(key_str.to_string(), data_slice.to_vec(), content_type_opt) {
        Ok(()) => 0,
        Err(_) => -1,
    }
}

/// Put value into cache with metadata
#[no_mangle]
pub extern "C" fn anx_cache_put_with_metadata(
    handle: *const CacheHandle,
    key: *const c_char,
    data: *const u8,
    data_len: usize,
    content_type: *const c_char,
    etag: *const c_char,
    last_modified: c_ulong,
    ttl_secs: c_ulong,
) -> c_int {
    if handle.is_null() || key.is_null() || data.is_null() {
        return -1;
    }
    
    let handle = unsafe { &*handle };
    let key_str = unsafe {
        match CStr::from_ptr(key).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    
    let data_slice = unsafe { std::slice::from_raw_parts(data, data_len) };
    
    let content_type_opt = if content_type.is_null() {
        None
    } else {
        unsafe {
            CStr::from_ptr(content_type).to_str().ok().map(|s| s.to_string())
        }
    };
    
    let etag_opt = if etag.is_null() {
        None
    } else {
        unsafe {
            CStr::from_ptr(etag).to_str().ok().map(|s| s.to_string())
        }
    };
    
    let last_modified_opt = if last_modified == 0 {
        None
    } else {
        Some(UNIX_EPOCH + std::time::Duration::from_secs(last_modified))
    };
    
    let ttl_opt = if ttl_secs == 0 {
        None
    } else {
        Some(std::time::Duration::from_secs(ttl_secs))
    };
    
    match handle.cache.put_with_metadata(
        key_str.to_string(),
        data_slice.to_vec(),
        content_type_opt,
        etag_opt,
        last_modified_opt,
        ttl_opt,
    ) {
        Ok(()) => 0,
        Err(_) => -1,
    }
}

/// Remove value from cache
#[no_mangle]
pub extern "C" fn anx_cache_remove(handle: *const CacheHandle, key: *const c_char) -> c_int {
    if handle.is_null() || key.is_null() {
        return -1;
    }
    
    let handle = unsafe { &*handle };
    let key_str = unsafe {
        match CStr::from_ptr(key).to_str() {
            Ok(s) => s,
            Err(_) => return -1,
        }
    };
    
    match handle.cache.remove(key_str) {
        Ok(()) => 0,
        Err(_) => -1,
    }
}

/// Clear all entries from cache
#[no_mangle]
pub extern "C" fn anx_cache_clear(handle: *const CacheHandle) {
    if handle.is_null() {
        return;
    }
    
    let handle = unsafe { &*handle };
    handle.cache.clear();
}

/// Get cache statistics
#[no_mangle]
pub extern "C" fn anx_cache_get_stats(handle: *const CacheHandle) -> *mut CacheStatsC {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    let stats = handle.cache.get_stats();
    
    let cache_stats = Box::new(CacheStatsC {
        hits: stats.hits,
        misses: stats.misses,
        puts: stats.puts,
        evictions: stats.evictions,
        current_size: stats.current_size,
        current_entries: stats.current_entries,
        hit_rate: stats.hit_rate(),
    });
    
    Box::into_raw(cache_stats)
}

/// Clean up expired entries
#[no_mangle]
pub extern "C" fn anx_cache_cleanup_expired(handle: *const CacheHandle) {
    if handle.is_null() {
        return;
    }
    
    let handle = unsafe { &*handle };
    handle.cache.cleanup_expired();
}

/// Generate ETag for content
#[no_mangle]
pub extern "C" fn anx_cache_generate_etag(data: *const u8, len: usize, last_modified: c_ulong) -> *mut c_char {
    if data.is_null() || len == 0 {
        return ptr::null_mut();
    }
    
    let data_slice = unsafe { std::slice::from_raw_parts(data, len) };
    let last_modified_opt = if last_modified == 0 {
        None
    } else {
        Some(UNIX_EPOCH + std::time::Duration::from_secs(last_modified))
    };
    
    let etag = Cache::generate_etag(data_slice, last_modified_opt);
    
    match CString::new(etag) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// Free cache handle
#[no_mangle]
pub extern "C" fn anx_cache_free(handle: *mut CacheHandle) {
    if !handle.is_null() {
        unsafe {
            Box::from_raw(handle);
        }
    }
}

/// Free cache response
#[no_mangle]
pub extern "C" fn anx_cache_response_free(response: *mut CacheResponseC) {
    if !response.is_null() {
        unsafe {
            let resp = Box::from_raw(response);
            if !resp.data.is_null() {
                Vec::from_raw_parts(resp.data, resp.data_len, resp.data_len);
            }
            if !resp.content_type.is_null() {
                CString::from_raw(resp.content_type);
            }
            if !resp.etag.is_null() {
                CString::from_raw(resp.etag);
            }
        }
    }
}

/// Free cache statistics
#[no_mangle]
pub extern "C" fn anx_cache_stats_free(stats: *mut CacheStatsC) {
    if !stats.is_null() {
        unsafe {
            Box::from_raw(stats);
        }
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

/// Free C string allocated by Rust
#[no_mangle]
pub extern "C" fn anx_free_string(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe {
            CString::from_raw(ptr);
        }
    }
}

/// Free byte array allocated by Rust
#[no_mangle]
pub extern "C" fn anx_free_bytes(ptr: *mut u8, len: usize) {
    if !ptr.is_null() && len > 0 {
        unsafe {
            Vec::from_raw_parts(ptr, len, len);
        }
    }
} 