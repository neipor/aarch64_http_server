//! Nginx configuration compatibility module for ANX HTTP Server

use super::{AnxConfig, ServerConfig, LocationConfig, LoggingConfig, SslConfig, ConfigError};
use std::collections::HashMap;

/// Parse Nginx-style configuration into AnxConfig
pub fn parse_nginx_config(content: &str) -> Result<AnxConfig, ConfigError> {
    let mut config = AnxConfig::default();
    let mut current_context = Context::Global;
    let mut current_location = LocationConfig {
        path: String::new(),
        root: None,
        index: None,
        proxy_pass: None,
        return_code: None,
        return_url: None,
        try_files: None,
        headers: None,
    };
    
    let lines = content.lines();
    
    for (line_num, line) in lines.enumerate() {
        let line = line.trim();
        
        // Skip empty lines and comments
        if line.is_empty() || line.starts_with('#') {
            continue;
        }
        
        // Parse directives
        if let Err(e) = parse_directive(line, &mut config, &mut current_context, &mut current_location) {
            return Err(ConfigError::Invalid(format!("Error on line {}: {}", line_num + 1, e)));
        }
    }
    
    config.validate()?;
    Ok(config)
}

#[derive(Debug, Clone)]
enum Context {
    Global,
    Server,
    Location,
}

fn parse_directive(
    line: &str,
    config: &mut AnxConfig,
    context: &mut Context,
    current_location: &mut LocationConfig,
) -> Result<(), String> {
    let line = line.trim_end_matches(';');
    let parts: Vec<&str> = line.split_whitespace().collect();
    
    if parts.is_empty() {
        return Ok(());
    }
    
    match parts[0] {
        "server" => {
            if line.contains('{') {
                *context = Context::Server;
                return Ok(());
            }
        }
        "location" => {
            if parts.len() >= 2 && line.contains('{') {
                *context = Context::Location;
                current_location.path = parts[1].to_string();
                return Ok(());
            }
        }
        "}" => {
            match context {
                Context::Server => *context = Context::Global,
                Context::Location => {
                    if !current_location.path.is_empty() {
                        config.locations.push(current_location.clone());
                        *current_location = LocationConfig {
                            path: String::new(),
                            root: None,
                            index: None,
                            proxy_pass: None,
                            return_code: None,
                            return_url: None,
                            try_files: None,
                            headers: None,
                        };
                    }
                    *context = Context::Server;
                }
                Context::Global => {}
            }
            return Ok(());
        }
        "events" | "http" => {
            if line.contains('{') {
                return Ok(());
            }
        }
        _ => {}
    }
    
    // Parse specific directives based on context
    match context {
        Context::Global => parse_global_directive(parts, config)?,
        Context::Server => parse_server_directive(parts, config)?,
        Context::Location => parse_location_directive(parts, current_location)?,
    }
    
    Ok(())
}

fn parse_global_directive(parts: Vec<&str>, config: &mut AnxConfig) -> Result<(), String> {
    match parts[0] {
        "worker_processes" => {
            if parts.len() >= 2 {
                if parts[1] == "auto" {
                    config.server.worker_processes = Some(num_cpus::get());
                } else {
                    config.server.worker_processes = Some(
                        parts[1].parse().map_err(|_| "Invalid worker_processes value")?
                    );
                }
            }
        }
        "error_log" => {
            if parts.len() >= 2 {
                if config.logging.is_none() {
                    config.logging = Some(LoggingConfig {
                        access_log: None,
                        error_log: Some(parts[1].to_string()),
                        log_level: None,
                    });
                } else {
                    config.logging.as_mut().unwrap().error_log = Some(parts[1].to_string());
                }
            }
        }
        _ => {}
    }
    Ok(())
}

fn parse_server_directive(parts: Vec<&str>, config: &mut AnxConfig) -> Result<(), String> {
    match parts[0] {
        "listen" => {
            if parts.len() >= 2 {
                if !config.server.listen.contains(&parts[1].to_string()) {
                    config.server.listen.push(parts[1].to_string());
                }
            }
        }
        "server_name" => {
            if parts.len() >= 2 {
                config.server.server_name = Some(parts[1].to_string());
            }
        }
        "root" => {
            if parts.len() >= 2 {
                config.server.root = parts[1].to_string();
            }
        }
        "index" => {
            if parts.len() >= 2 {
                config.server.index = parts[1..].iter().map(|s| s.to_string()).collect();
            }
        }
        "keepalive_timeout" => {
            if parts.len() >= 2 {
                config.server.keepalive_timeout = Some(
                    parts[1].parse().map_err(|_| "Invalid keepalive_timeout value")?
                );
            }
        }
        "client_max_body_size" => {
            if parts.len() >= 2 {
                config.server.client_max_body_size = Some(parts[1].to_string());
            }
        }
        "access_log" => {
            if parts.len() >= 2 {
                if config.logging.is_none() {
                    config.logging = Some(LoggingConfig {
                        access_log: Some(parts[1].to_string()),
                        error_log: None,
                        log_level: None,
                    });
                } else {
                    config.logging.as_mut().unwrap().access_log = Some(parts[1].to_string());
                }
            }
        }
        "ssl_certificate" => {
            if parts.len() >= 2 {
                if config.ssl.is_none() {
                    config.ssl = Some(SslConfig {
                        certificate: parts[1].to_string(),
                        private_key: String::new(),
                        protocols: vec!["TLSv1.2".to_string(), "TLSv1.3".to_string()],
                        ciphers: None,
                    });
                } else {
                    config.ssl.as_mut().unwrap().certificate = parts[1].to_string();
                }
            }
        }
        "ssl_certificate_key" => {
            if parts.len() >= 2 {
                if config.ssl.is_none() {
                    config.ssl = Some(SslConfig {
                        certificate: String::new(),
                        private_key: parts[1].to_string(),
                        protocols: vec!["TLSv1.2".to_string(), "TLSv1.3".to_string()],
                        ciphers: None,
                    });
                } else {
                    config.ssl.as_mut().unwrap().private_key = parts[1].to_string();
                }
            }
        }
        "ssl_protocols" => {
            if parts.len() >= 2 {
                let protocols = parts[1..].iter().map(|s| s.to_string()).collect();
                if config.ssl.is_none() {
                    config.ssl = Some(SslConfig {
                        certificate: String::new(),
                        private_key: String::new(),
                        protocols,
                        ciphers: None,
                    });
                } else {
                    config.ssl.as_mut().unwrap().protocols = protocols;
                }
            }
        }
        _ => {}
    }
    Ok(())
}

fn parse_location_directive(parts: Vec<&str>, location: &mut LocationConfig) -> Result<(), String> {
    match parts[0] {
        "root" => {
            if parts.len() >= 2 {
                location.root = Some(parts[1].to_string());
            }
        }
        "index" => {
            if parts.len() >= 2 {
                location.index = Some(parts[1..].iter().map(|s| s.to_string()).collect());
            }
        }
        "proxy_pass" => {
            if parts.len() >= 2 {
                location.proxy_pass = Some(parts[1].to_string());
            }
        }
        "return" => {
            if parts.len() >= 2 {
                location.return_code = Some(
                    parts[1].parse().map_err(|_| "Invalid return code")?
                );
                if parts.len() >= 3 {
                    location.return_url = Some(parts[2].to_string());
                }
            }
        }
        "try_files" => {
            if parts.len() >= 2 {
                location.try_files = Some(parts[1..].iter().map(|s| s.to_string()).collect());
            }
        }
        "add_header" => {
            if parts.len() >= 3 {
                if location.headers.is_none() {
                    location.headers = Some(HashMap::new());
                }
                location.headers.as_mut().unwrap().insert(
                    parts[1].to_string(),
                    parts[2..].join(" ")
                );
            }
        }
        _ => {}
    }
    Ok(())
}

/// Convert AnxConfig to Nginx-style configuration
pub fn to_nginx_config(config: &AnxConfig) -> String {
    let mut nginx_config = String::new();
    
    // Global directives
    if let Some(processes) = config.server.worker_processes {
        nginx_config.push_str(&format!("worker_processes {};\n", processes));
    }
    
    if let Some(connections) = config.server.worker_connections {
        nginx_config.push_str(&format!("events {{\n    worker_connections {};\n}}\n\n", connections));
    }
    
    nginx_config.push_str("http {\n");
    
    // Server block
    nginx_config.push_str("    server {\n");
    
    // Listen directives
    for listen in &config.server.listen {
        nginx_config.push_str(&format!("        listen {};\n", listen));
    }
    
    // Server name
    if let Some(ref server_name) = config.server.server_name {
        nginx_config.push_str(&format!("        server_name {};\n", server_name));
    }
    
    // Root and index
    nginx_config.push_str(&format!("        root {};\n", config.server.root));
    nginx_config.push_str(&format!("        index {};\n", config.server.index.join(" ")));
    
    // SSL configuration
    if let Some(ref ssl) = config.ssl {
        nginx_config.push_str(&format!("        ssl_certificate {};\n", ssl.certificate));
        nginx_config.push_str(&format!("        ssl_certificate_key {};\n", ssl.private_key));
        nginx_config.push_str(&format!("        ssl_protocols {};\n", ssl.protocols.join(" ")));
        
        if let Some(ref ciphers) = ssl.ciphers {
            nginx_config.push_str(&format!("        ssl_ciphers {};\n", ciphers));
        }
    }
    
    // Logging
    if let Some(ref logging) = config.logging {
        if let Some(ref access_log) = logging.access_log {
            nginx_config.push_str(&format!("        access_log {};\n", access_log));
        }
        if let Some(ref error_log) = logging.error_log {
            nginx_config.push_str(&format!("        error_log {};\n", error_log));
        }
    }
    
    // Locations
    for location in &config.locations {
        nginx_config.push_str(&format!("        location {} {{\n", location.path));
        
        if let Some(ref root) = location.root {
            nginx_config.push_str(&format!("            root {};\n", root));
        }
        
        if let Some(ref index) = location.index {
            nginx_config.push_str(&format!("            index {};\n", index.join(" ")));
        }
        
        if let Some(ref proxy_pass) = location.proxy_pass {
            nginx_config.push_str(&format!("            proxy_pass {};\n", proxy_pass));
        }
        
        if let Some(code) = location.return_code {
            if let Some(ref url) = location.return_url {
                nginx_config.push_str(&format!("            return {} {};\n", code, url));
            } else {
                nginx_config.push_str(&format!("            return {};\n", code));
            }
        }
        
        if let Some(ref try_files) = location.try_files {
            nginx_config.push_str(&format!("            try_files {};\n", try_files.join(" ")));
        }
        
        if let Some(ref headers) = location.headers {
            for (name, value) in headers {
                nginx_config.push_str(&format!("            add_header {} {};\n", name, value));
            }
        }
        
        nginx_config.push_str("        }\n");
    }
    
    nginx_config.push_str("    }\n");
    nginx_config.push_str("}\n");
    
    nginx_config
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_simple_nginx_config() {
        let nginx_config = r#"
worker_processes auto;

events {
    worker_connections 1024;
}

http {
    server {
        listen 80;
        server_name example.com;
        root /var/www/html;
        index index.html index.htm;
        
        location / {
            try_files $uri $uri/ =404;
        }
    }
}
"#;
        
        let config = parse_nginx_config(nginx_config).unwrap();
        assert_eq!(config.server.listen, vec!["80"]);
        assert_eq!(config.server.server_name, Some("example.com".to_string()));
        assert_eq!(config.server.root, "/var/www/html");
        assert_eq!(config.locations.len(), 1);
        assert_eq!(config.locations[0].path, "/");
    }
    
    #[test]
    fn test_convert_to_nginx_config() {
        let config = AnxConfig::default();
        let nginx_config = to_nginx_config(&config);
        
        assert!(nginx_config.contains("listen 80;"));
        assert!(nginx_config.contains("root /var/www/html;"));
        assert!(nginx_config.contains("index index.html index.htm;"));
    }
} 