use std::path::PathBuf;
use std::collections::HashMap;
use std::env;
use std::str::FromStr;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum CliError {
    #[error("无效的参数: {0}")]
    InvalidArgument(String),
    #[error("缺少必需参数: {0}")]
    MissingArgument(String),
    #[error("无效的端口号: {0}")]
    InvalidPort(String),
    #[error("无效的路径: {0}")]
    InvalidPath(String),
    #[error("无效的URL: {0}")]
    InvalidUrl(String),
    #[error("配置文件错误: {0}")]
    ConfigError(String),
}

/// 服务器运行模式
#[derive(Debug, Clone, PartialEq)]
pub enum ServerMode {
    /// 静态文件服务器
    Static,
    /// 反向代理
    Proxy,
    /// 混合模式 (静态文件 + 反向代理)
    Hybrid,
}

/// 反向代理配置
#[derive(Debug, Clone)]
pub struct ProxyConfig {
    pub target_url: String,
    pub path_prefix: Option<String>,
    pub timeout: Option<u64>,
    pub health_check: bool,
}

/// SSL配置
#[derive(Debug, Clone)]
pub struct SslConfig {
    pub cert_file: PathBuf,
    pub key_file: PathBuf,
    pub enabled: bool,
}

/// 命令行参数配置
#[derive(Debug, Clone)]
pub struct CliConfig {
    // 基本配置
    pub port: u16,
    pub host: String,
    pub mode: ServerMode,
    
    // 静态文件配置
    pub static_dir: Option<PathBuf>,
    pub index_files: Vec<String>,
    pub auto_index: bool,
    
    // 反向代理配置
    pub proxy_configs: Vec<ProxyConfig>,
    
    // SSL配置
    pub ssl: Option<SslConfig>,
    
    // 日志配置
    pub log_level: String,
    pub log_file: Option<PathBuf>,
    
    // 缓存配置
    pub cache_enabled: bool,
    pub cache_size: usize,
    pub cache_ttl: u64,
    
    // 性能配置
    pub threads: usize,
    pub max_connections: usize,
    
    // 其他配置
    pub config_file: Option<PathBuf>,
    pub daemon: bool,
    pub pid_file: Option<PathBuf>,
}

impl Default for CliConfig {
    fn default() -> Self {
        Self {
            port: 8080,
            host: "0.0.0.0".to_string(),
            mode: ServerMode::Static,
            static_dir: None,
            index_files: vec!["index.html".to_string(), "index.htm".to_string()],
            auto_index: true,
            proxy_configs: Vec::new(),
            ssl: None,
            log_level: "info".to_string(),
            log_file: None,
            cache_enabled: true,
            cache_size: 100 * 1024 * 1024, // 100MB
            cache_ttl: 3600, // 1小时
            threads: num_cpus::get(),
            max_connections: 1000,
            config_file: None,
            daemon: false,
            pid_file: None,
        }
    }
}

/// 命令行参数解析器
pub struct CliParser {
    args: Vec<String>,
    config: CliConfig,
}

impl CliParser {
    pub fn new() -> Self {
        let args: Vec<String> = env::args().collect();
        Self {
            args,
            config: CliConfig::default(),
        }
    }
    
    /// 解析命令行参数
    pub fn parse(&mut self) -> Result<CliConfig, CliError> {
        let mut i = 1;
        while i < self.args.len() {
            let arg = &self.args[i];
            
            match arg.as_str() {
                // 帮助信息
                "-h" | "--help" => {
                    self.print_help();
                    std::process::exit(0);
                }
                
                // 版本信息
                "-v" | "--version" => {
                    self.print_version();
                    std::process::exit(0);
                }
                
                // 端口配置
                "-p" | "--port" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("port".to_string()));
                    }
                    let port = self.args[i].parse::<u16>()
                        .map_err(|_| CliError::InvalidPort(self.args[i].clone()))?;
                    self.config.port = port;
                }
                
                // 主机配置
                "--host" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("host".to_string()));
                    }
                    self.config.host = self.args[i].clone();
                }
                
                // 静态文件目录
                "-d" | "--dir" | "--static-dir" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("static directory".to_string()));
                    }
                    let path = PathBuf::from(&self.args[i]);
                    if !path.exists() {
                        return Err(CliError::InvalidPath(self.args[i].clone()));
                    }
                    self.config.static_dir = Some(path);
                    self.config.mode = ServerMode::Static;
                }
                
                // 反向代理
                "--proxy" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("proxy target".to_string()));
                    }
                    let target = self.args[i].clone();
                    if !target.starts_with("http://") && !target.starts_with("https://") {
                        return Err(CliError::InvalidUrl(target));
                    }
                    
                    let mut proxy_config = ProxyConfig {
                        target_url: target,
                        path_prefix: Some("/".to_string()), // 默认路径前缀为 "/"
                        timeout: Some(30),
                        health_check: true,
                    };
                    
                    // 检查是否有路径前缀
                    if i + 1 < self.args.len() && !self.args[i + 1].starts_with('-') {
                        i += 1;
                        proxy_config.path_prefix = Some(self.args[i].clone());
                    }
                    
                    self.config.proxy_configs.push(proxy_config);
                    if self.config.mode == ServerMode::Static {
                        self.config.mode = ServerMode::Hybrid;
                    } else {
                        self.config.mode = ServerMode::Proxy;
                    }
                }
                
                // SSL配置
                "--ssl-cert" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("SSL certificate".to_string()));
                    }
                    let cert_path = PathBuf::from(&self.args[i]);
                    if !cert_path.exists() {
                        return Err(CliError::InvalidPath(self.args[i].clone()));
                    }
                    
                    if self.config.ssl.is_none() {
                        self.config.ssl = Some(SslConfig {
                            cert_file: cert_path,
                            key_file: PathBuf::new(),
                            enabled: true,
                        });
                    } else {
                        self.config.ssl.as_mut().unwrap().cert_file = cert_path;
                    }
                }
                
                "--ssl-key" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("SSL key".to_string()));
                    }
                    let key_path = PathBuf::from(&self.args[i]);
                    if !key_path.exists() {
                        return Err(CliError::InvalidPath(self.args[i].clone()));
                    }
                    
                    if self.config.ssl.is_none() {
                        self.config.ssl = Some(SslConfig {
                            cert_file: PathBuf::new(),
                            key_file: key_path,
                            enabled: true,
                        });
                    } else {
                        self.config.ssl.as_mut().unwrap().key_file = key_path;
                    }
                }
                
                // 日志配置
                "--log-level" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("log level".to_string()));
                    }
                    let level = self.args[i].clone();
                    if !["debug", "info", "warn", "error"].contains(&level.as_str()) {
                        return Err(CliError::InvalidArgument(format!("Invalid log level: {}", level)));
                    }
                    self.config.log_level = level;
                }
                
                "--log-file" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("log file".to_string()));
                    }
                    self.config.log_file = Some(PathBuf::from(&self.args[i]));
                }
                
                // 缓存配置
                "--no-cache" => {
                    self.config.cache_enabled = false;
                }
                
                "--cache-size" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("cache size".to_string()));
                    }
                    let size = self.parse_size(&self.args[i])?;
                    self.config.cache_size = size;
                }
                
                "--cache-ttl" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("cache TTL".to_string()));
                    }
                    let ttl = self.args[i].parse::<u64>()
                        .map_err(|_| CliError::InvalidArgument(format!("Invalid TTL: {}", self.args[i])))?;
                    self.config.cache_ttl = ttl;
                }
                
                // 性能配置
                "--threads" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("threads".to_string()));
                    }
                    let threads = self.args[i].parse::<usize>()
                        .map_err(|_| CliError::InvalidArgument(format!("Invalid threads: {}", self.args[i])))?;
                    self.config.threads = threads;
                }
                
                "--max-connections" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("max connections".to_string()));
                    }
                    let max_conn = self.args[i].parse::<usize>()
                        .map_err(|_| CliError::InvalidArgument(format!("Invalid max connections: {}", self.args[i])))?;
                    self.config.max_connections = max_conn;
                }
                
                // 配置文件
                "-c" | "--config" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("config file".to_string()));
                    }
                    self.config.config_file = Some(PathBuf::from(&self.args[i]));
                }
                
                // 守护进程
                "-D" | "--daemon" => {
                    self.config.daemon = true;
                }
                
                "--pid-file" => {
                    i += 1;
                    if i >= self.args.len() {
                        return Err(CliError::MissingArgument("PID file".to_string()));
                    }
                    self.config.pid_file = Some(PathBuf::from(&self.args[i]));
                }
                
                // 其他选项
                "--auto-index" => {
                    self.config.auto_index = true;
                }
                
                "--no-auto-index" => {
                    self.config.auto_index = false;
                }
                
                _ => {
                    return Err(CliError::InvalidArgument(arg.clone()));
                }
            }
            
            i += 1;
        }
        
        // 验证配置
        self.validate_config()?;
        
        Ok(self.config.clone())
    }
    
    /// 验证配置
    fn validate_config(&self) -> Result<(), CliError> {
        // 检查静态文件目录
        if self.config.mode == ServerMode::Static && self.config.static_dir.is_none() {
            return Err(CliError::MissingArgument("static directory (use -d or --dir)".to_string()));
        }
        
        // 检查反向代理配置
        if self.config.mode == ServerMode::Proxy && self.config.proxy_configs.is_empty() {
            return Err(CliError::MissingArgument("proxy target (use --proxy)".to_string()));
        }
        
        // 检查SSL配置
        if let Some(ssl) = &self.config.ssl {
            if ssl.cert_file.to_string_lossy().is_empty() || ssl.key_file.to_string_lossy().is_empty() {
                return Err(CliError::MissingArgument("SSL certificate and key files".to_string()));
            }
        }
        
        Ok(())
    }
    
    /// 解析大小字符串 (如 "100MB", "1GB")
    fn parse_size(&self, size_str: &str) -> Result<usize, CliError> {
        let size_str = size_str.to_lowercase();
        let (number, unit) = size_str.split_at(
            size_str.chars().take_while(|c| c.is_digit(10)).count()
        );
        
        let number: usize = number.parse()
            .map_err(|_| CliError::InvalidArgument(format!("Invalid size number: {}", number)))?;
        
        let multiplier = match unit {
            "" | "b" => 1,
            "kb" => 1024,
            "mb" => 1024 * 1024,
            "gb" => 1024 * 1024 * 1024,
            _ => return Err(CliError::InvalidArgument(format!("Invalid size unit: {}", unit))),
        };
        
        Ok(number * multiplier)
    }
    
    /// 打印帮助信息
    fn print_help(&self) {
        println!("ASM HTTP Server v1.1.0+");
        println!("高性能混合架构HTTP服务器");
        println!();
        println!("用法: asm_server [选项]");
        println!();
        println!("基本选项:");
        println!("  -h, --help              显示此帮助信息");
        println!("  -v, --version           显示版本信息");
        println!("  -p, --port PORT         指定端口 (默认: 8080)");
        println!("  --host HOST             指定主机 (默认: 0.0.0.0)");
        println!();
        println!("静态文件服务器:");
        println!("  -d, --dir, --static-dir DIR  指定静态文件目录");
        println!("  --auto-index                 启用目录索引 (默认)");
        println!("  --no-auto-index             禁用目录索引");
        println!();
        println!("反向代理:");
        println!("  --proxy URL [PATH]      配置反向代理到URL，可选PATH前缀");
        println!();
        println!("SSL配置:");
        println!("  --ssl-cert FILE         指定SSL证书文件");
        println!("  --ssl-key FILE          指定SSL私钥文件");
        println!();
        println!("日志配置:");
        println!("  --log-level LEVEL       设置日志级别 (debug|info|warn|error)");
        println!("  --log-file FILE         指定日志文件");
        println!();
        println!("缓存配置:");
        println!("  --no-cache              禁用缓存");
        println!("  --cache-size SIZE       设置缓存大小 (如: 100MB, 1GB)");
        println!("  --cache-ttl SECONDS    设置缓存TTL (默认: 3600)");
        println!();
        println!("性能配置:");
        println!("  --threads NUM           设置工作线程数");
        println!("  --max-connections NUM   设置最大连接数");
        println!();
        println!("其他选项:");
        println!("  -c, --config FILE      指定配置文件");
        println!("  -D, --daemon           以守护进程模式运行");
        println!("  --pid-file FILE        指定PID文件");
        println!();
        println!("示例:");
        println!("  # 静态文件服务器");
        println!("  asm_server -d /var/www/html -p 80");
        println!();
        println!("  # 反向代理");
        println!("  asm_server --proxy http://backend:8080 -p 80");
        println!();
        println!("  # 混合模式");
        println!("  asm_server -d /var/www/html --proxy http://api:8080 /api");
        println!();
        println!("  # SSL服务器");
        println!("  asm_server -d /var/www/html --ssl-cert cert.pem --ssl-key key.pem");
    }
    
    /// 打印版本信息
    fn print_version(&self) {
        println!("ASM HTTP Server v1.1.0+");
        println!("高性能混合架构HTTP服务器");
        println!("基于C语言和Rust混合架构");
    }
}

impl Default for CliParser {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_default_config() {
        let config = CliConfig::default();
        assert_eq!(config.port, 8080);
        assert_eq!(config.host, "0.0.0.0");
        assert_eq!(config.mode, ServerMode::Static);
        assert!(config.cache_enabled);
    }
    
    #[test]
    fn test_size_parsing() {
        let parser = CliParser::new();
        assert_eq!(parser.parse_size("1024").unwrap(), 1024);
        assert_eq!(parser.parse_size("1MB").unwrap(), 1024 * 1024);
        assert_eq!(parser.parse_size("1GB").unwrap(), 1024 * 1024 * 1024);
    }
} 