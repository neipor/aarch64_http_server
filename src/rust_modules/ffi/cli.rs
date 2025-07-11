use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::ptr;
use crate::cli::{CliConfig, CliParser, CliError};

/// CLI配置句柄
pub struct CliConfigHandle {
    pub config: CliConfig,
}

/// 创建CLI配置解析器
#[no_mangle]
pub extern "C" fn anx_cli_parser_create() -> *mut CliParser {
    let parser = CliParser::new();
    Box::into_raw(Box::new(parser))
}

/// 解析命令行参数
#[no_mangle]
pub extern "C" fn anx_cli_parse_args(parser: *mut CliParser) -> *mut CliConfigHandle {
    if parser.is_null() {
        return ptr::null_mut();
    }
    
    let parser = unsafe { &mut *parser };
    match parser.parse() {
        Ok(config) => {
            let handle = CliConfigHandle { config };
            Box::into_raw(Box::new(handle))
        }
        Err(_) => ptr::null_mut(),
    }
}

/// 获取端口号
#[no_mangle]
pub extern "C" fn anx_cli_get_port(handle: *const CliConfigHandle) -> u16 {
    if handle.is_null() {
        return 8080;
    }
    
    let handle = unsafe { &*handle };
    handle.config.port
}

/// 获取主机地址
#[no_mangle]
pub extern "C" fn anx_cli_get_host(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match CString::new(handle.config.host.clone()) {
        Ok(s) => s.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// 获取静态文件目录
#[no_mangle]
pub extern "C" fn anx_cli_get_static_dir(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match &handle.config.static_dir {
        Some(path) => {
            match CString::new(path.to_string_lossy().to_string()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 获取代理目标数量
#[no_mangle]
pub extern "C" fn anx_cli_get_proxy_count(handle: *const CliConfigHandle) -> usize {
    if handle.is_null() {
        return 0;
    }
    
    let handle = unsafe { &*handle };
    handle.config.proxy_configs.len()
}

/// 获取代理目标URL
#[no_mangle]
pub extern "C" fn anx_cli_get_proxy_url(handle: *const CliConfigHandle, index: usize) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    if index >= handle.config.proxy_configs.len() {
        return ptr::null_mut();
    }
    
    let proxy = &handle.config.proxy_configs[index];
    match CString::new(proxy.target_url.clone()) {
        Ok(s) => s.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// 获取代理路径前缀
#[no_mangle]
pub extern "C" fn anx_cli_get_proxy_path_prefix(handle: *const CliConfigHandle, index: usize) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    if index >= handle.config.proxy_configs.len() {
        return ptr::null_mut();
    }
    
    let proxy = &handle.config.proxy_configs[index];
    match &proxy.path_prefix {
        Some(prefix) => {
            match CString::new(prefix.clone()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 检查是否启用SSL
#[no_mangle]
pub extern "C" fn anx_cli_is_ssl_enabled(handle: *const CliConfigHandle) -> bool {
    if handle.is_null() {
        return false;
    }
    
    let handle = unsafe { &*handle };
    handle.config.ssl.is_some()
}

/// 获取SSL证书文件路径
#[no_mangle]
pub extern "C" fn anx_cli_get_ssl_cert_file(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match &handle.config.ssl {
        Some(ssl) => {
            match CString::new(ssl.cert_file.to_string_lossy().to_string()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 获取SSL私钥文件路径
#[no_mangle]
pub extern "C" fn anx_cli_get_ssl_key_file(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match &handle.config.ssl {
        Some(ssl) => {
            match CString::new(ssl.key_file.to_string_lossy().to_string()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 获取日志级别
#[no_mangle]
pub extern "C" fn anx_cli_get_log_level(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match CString::new(handle.config.log_level.clone()) {
        Ok(s) => s.into_raw(),
        Err(_) => ptr::null_mut(),
    }
}

/// 获取日志文件路径
#[no_mangle]
pub extern "C" fn anx_cli_get_log_file(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match &handle.config.log_file {
        Some(path) => {
            match CString::new(path.to_string_lossy().to_string()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 检查是否启用缓存
#[no_mangle]
pub extern "C" fn anx_cli_is_cache_enabled(handle: *const CliConfigHandle) -> bool {
    if handle.is_null() {
        return false;
    }
    
    let handle = unsafe { &*handle };
    handle.config.cache_enabled
}

/// 获取缓存大小
#[no_mangle]
pub extern "C" fn anx_cli_get_cache_size(handle: *const CliConfigHandle) -> usize {
    if handle.is_null() {
        return 0;
    }
    
    let handle = unsafe { &*handle };
    handle.config.cache_size
}

/// 获取缓存TTL
#[no_mangle]
pub extern "C" fn anx_cli_get_cache_ttl(handle: *const CliConfigHandle) -> u64 {
    if handle.is_null() {
        return 3600;
    }
    
    let handle = unsafe { &*handle };
    handle.config.cache_ttl
}

/// 获取线程数
#[no_mangle]
pub extern "C" fn anx_cli_get_threads(handle: *const CliConfigHandle) -> usize {
    if handle.is_null() {
        return 1;
    }
    
    let handle = unsafe { &*handle };
    handle.config.threads
}

/// 获取最大连接数
#[no_mangle]
pub extern "C" fn anx_cli_get_max_connections(handle: *const CliConfigHandle) -> usize {
    if handle.is_null() {
        return 1000;
    }
    
    let handle = unsafe { &*handle };
    handle.config.max_connections
}

/// 检查是否为守护进程模式
#[no_mangle]
pub extern "C" fn anx_cli_is_daemon(handle: *const CliConfigHandle) -> bool {
    if handle.is_null() {
        return false;
    }
    
    let handle = unsafe { &*handle };
    handle.config.daemon
}

/// 获取PID文件路径
#[no_mangle]
pub extern "C" fn anx_cli_get_pid_file(handle: *const CliConfigHandle) -> *mut c_char {
    if handle.is_null() {
        return ptr::null_mut();
    }
    
    let handle = unsafe { &*handle };
    match &handle.config.pid_file {
        Some(path) => {
            match CString::new(path.to_string_lossy().to_string()) {
                Ok(s) => s.into_raw(),
                Err(_) => ptr::null_mut(),
            }
        }
        None => ptr::null_mut(),
    }
}

/// 释放CLI配置句柄
#[no_mangle]
pub extern "C" fn anx_cli_config_free(handle: *mut CliConfigHandle) {
    if !handle.is_null() {
        unsafe {
            let _ = Box::from_raw(handle);
        }
    }
}

/// 释放CLI解析器
#[no_mangle]
pub extern "C" fn anx_cli_parser_free(parser: *mut CliParser) {
    if !parser.is_null() {
        unsafe {
            let _ = Box::from_raw(parser);
        }
    }
} 