//! TOML configuration parser for ANX HTTP Server

use super::{AnxConfig, ConfigError};
use std::collections::HashMap;

/// Parse TOML configuration string into AnxConfig
pub fn parse_toml(content: &str) -> Result<AnxConfig, ConfigError> {
    let config: AnxConfig = toml::from_str(content)?;
    config.validate()?;
    Ok(config)
}

/// Generate example TOML configuration
pub fn generate_example_config() -> String {
    r#"# ANX HTTP Server Configuration
# This is an example configuration file in TOML format

[server]
listen = ["80", "443"]
server_name = "example.com"
root = "/var/www/html"
index = ["index.html", "index.htm"]
worker_processes = 4
worker_connections = 1024
keepalive_timeout = 65
client_max_body_size = "10M"

[logging]
access_log = "/var/log/anx/access.log"
error_log = "/var/log/anx/error.log"
log_level = "info"

[ssl]
certificate = "/etc/ssl/certs/example.com.crt"
private_key = "/etc/ssl/private/example.com.key"
protocols = ["TLSv1.2", "TLSv1.3"]
ciphers = "ECDHE-RSA-AES128-GCM-SHA256:ECDHE-RSA-AES256-GCM-SHA384"

[proxy]
proxy_timeout = 60
proxy_connect_timeout = 5

[proxy.upstream]
backend = ["127.0.0.1:8001", "127.0.0.1:8002"]
api = ["127.0.0.1:9001"]

[cache]
enabled = true
max_size = "100M"
ttl = 3600
keys_zone = "main:10m"

[[locations]]
path = "/"
root = "/var/www/html"
index = ["index.html"]

[[locations]]
path = "/api"
proxy_pass = "http://backend"

[[locations]]
path = "/static"
root = "/var/www/static"
try_files = ["$uri", "$uri/", "@fallback"]

[locations.headers]
"X-Frame-Options" = "SAMEORIGIN"
"X-Content-Type-Options" = "nosniff"
"#.to_string()
}

/// Validate TOML configuration structure
pub fn validate_toml_structure(content: &str) -> Result<(), ConfigError> {
    // First, try to parse as raw TOML to check syntax
    let _: toml::Value = toml::from_str(content)
        .map_err(|e| ConfigError::Invalid(format!("Invalid TOML syntax: {}", e)))?;
    
    // Then try to parse as AnxConfig to check structure
    let _: AnxConfig = toml::from_str(content)?;
    
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_minimal_config() {
        let toml_content = r#"
[server]
listen = ["80"]
root = "/var/www/html"
index = ["index.html"]
"#;
        
        let config = parse_toml(toml_content).unwrap();
        assert_eq!(config.server.listen, vec!["80"]);
        assert_eq!(config.server.root, "/var/www/html");
        assert_eq!(config.server.index, vec!["index.html"]);
    }
    
    #[test]
    fn test_parse_complete_config() {
        let toml_content = generate_example_config();
        let config = parse_toml(&toml_content).unwrap();
        
        assert_eq!(config.server.listen, vec!["80", "443"]);
        assert_eq!(config.server.server_name, Some("example.com".to_string()));
        assert!(config.ssl.is_some());
        assert!(config.proxy.is_some());
        assert!(config.cache.is_some());
        assert_eq!(config.locations.len(), 3);
    }
    
    #[test]
    fn test_invalid_config() {
        let invalid_toml = r#"
[server]
# missing required fields
"#;
        
        assert!(parse_toml(invalid_toml).is_err());
    }
    
    #[test]
    fn test_validate_structure() {
        let valid_toml = r#"
[server]
listen = ["80"]
root = "/var/www/html"
index = ["index.html"]
"#;
        
        assert!(validate_toml_structure(valid_toml).is_ok());
        
        let invalid_toml = r#"
[server
# missing closing bracket
"#;
        
        assert!(validate_toml_structure(invalid_toml).is_err());
    }
} 