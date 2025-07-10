//! Configuration module for ANX HTTP Server
//! 
//! This module provides type-safe configuration parsing for TOML files
//! and compatibility with Nginx configuration format.

use serde::{Deserialize, Serialize};
use std::collections::HashMap;
use std::fs;
use std::path::Path;
use thiserror::Error;

pub mod nginx_compat;
pub mod toml_parser;

/// Main configuration structure for ANX server
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct AnxConfig {
    pub server: ServerConfig,
    pub logging: Option<LoggingConfig>,
    pub ssl: Option<SslConfig>,
    pub proxy: Option<ProxyConfig>,
    pub cache: Option<CacheConfig>,
    #[serde(default)]
    pub locations: Vec<LocationConfig>,
}

/// Server configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ServerConfig {
    pub listen: Vec<String>,
    pub server_name: Option<String>,
    pub root: String,
    pub index: Vec<String>,
    pub worker_processes: Option<usize>,
    pub worker_connections: Option<usize>,
    pub keepalive_timeout: Option<u64>,
    pub client_max_body_size: Option<String>,
}

/// Logging configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LoggingConfig {
    pub access_log: Option<String>,
    pub error_log: Option<String>,
    pub log_level: Option<String>,
}

/// SSL configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SslConfig {
    pub certificate: String,
    pub private_key: String,
    pub protocols: Vec<String>,
    pub ciphers: Option<String>,
}

/// Proxy configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct ProxyConfig {
    pub upstream: HashMap<String, Vec<String>>,
    pub proxy_timeout: Option<u64>,
    pub proxy_connect_timeout: Option<u64>,
}

/// Cache configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct CacheConfig {
    pub enabled: bool,
    pub max_size: Option<String>,
    pub ttl: Option<u64>,
    pub keys_zone: Option<String>,
}

/// Location configuration
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct LocationConfig {
    pub path: String,
    pub root: Option<String>,
    pub index: Option<Vec<String>>,
    pub proxy_pass: Option<String>,
    pub return_code: Option<u16>,
    pub return_url: Option<String>,
    pub try_files: Option<Vec<String>>,
    pub headers: Option<HashMap<String, String>>,
}

/// Configuration errors
#[derive(Error, Debug)]
pub enum ConfigError {
    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),
    #[error("TOML parsing error: {0}")]
    Toml(#[from] toml::de::Error),
    #[error("Invalid configuration: {0}")]
    Invalid(String),
    #[error("File not found: {0}")]
    NotFound(String),
}

impl Default for AnxConfig {
    fn default() -> Self {
        Self {
            server: ServerConfig {
                listen: vec!["80".to_string()],
                server_name: None,
                root: "/var/www/html".to_string(),
                index: vec!["index.html".to_string(), "index.htm".to_string()],
                worker_processes: Some(1),
                worker_connections: Some(1024),
                keepalive_timeout: Some(65),
                client_max_body_size: Some("1M".to_string()),
            },
            logging: Some(LoggingConfig {
                access_log: Some("/var/log/anx/access.log".to_string()),
                error_log: Some("/var/log/anx/error.log".to_string()),
                log_level: Some("info".to_string()),
            }),
            ssl: None,
            proxy: None,
            cache: None,
            locations: vec![],
        }
    }
}

impl AnxConfig {
    /// Load configuration from a TOML file
    pub fn from_toml_file<P: AsRef<Path>>(path: P) -> Result<Self, ConfigError> {
        let content = fs::read_to_string(&path)
            .map_err(|_| ConfigError::NotFound(path.as_ref().to_string_lossy().to_string()))?;
        
        toml_parser::parse_toml(&content)
    }
    
    /// Load configuration from a Nginx-style configuration file
    pub fn from_nginx_file<P: AsRef<Path>>(path: P) -> Result<Self, ConfigError> {
        let content = fs::read_to_string(&path)
            .map_err(|_| ConfigError::NotFound(path.as_ref().to_string_lossy().to_string()))?;
        
        nginx_compat::parse_nginx_config(&content)
    }
    
    /// Validate the configuration
    pub fn validate(&self) -> Result<(), ConfigError> {
        if self.server.listen.is_empty() {
            return Err(ConfigError::Invalid("Server must have at least one listen address".to_string()));
        }
        
        if self.server.root.is_empty() {
            return Err(ConfigError::Invalid("Server root directory cannot be empty".to_string()));
        }
        
        if self.server.index.is_empty() {
            return Err(ConfigError::Invalid("Server must have at least one index file".to_string()));
        }
        
        // Validate SSL configuration if present
        if let Some(ref ssl) = self.ssl {
            if ssl.certificate.is_empty() {
                return Err(ConfigError::Invalid("SSL certificate path cannot be empty".to_string()));
            }
            if ssl.private_key.is_empty() {
                return Err(ConfigError::Invalid("SSL private key path cannot be empty".to_string()));
            }
        }
        
        // Validate locations
        for location in &self.locations {
            if location.path.is_empty() {
                return Err(ConfigError::Invalid("Location path cannot be empty".to_string()));
            }
        }
        
        Ok(())
    }
    
    /// Convert configuration to TOML string
    pub fn to_toml(&self) -> Result<String, ConfigError> {
        toml::to_string_pretty(self)
            .map_err(|e| ConfigError::Invalid(format!("Failed to serialize to TOML: {}", e)))
    }
} 