//! HTTP request parser module for ANX HTTP Server
//! 
//! This module provides safe HTTP request parsing functionality.

use std::collections::HashMap;
use std::str::FromStr;
use std::fmt;
use thiserror::Error;

/// HTTP request structure
#[derive(Debug, Clone)]
pub struct HttpRequest {
    pub method: String,
    pub uri: String,
    pub version: String,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
}

/// HTTP response structure
#[derive(Debug, Clone)]
pub struct HttpResponse {
    pub status_code: u16,
    pub reason_phrase: String,
    pub version: String,
    pub headers: HashMap<String, String>,
    pub body: Vec<u8>,
}

/// HTTP parsing errors
#[derive(Error, Debug)]
pub enum HttpParseError {
    #[error("Invalid HTTP request: {0}")]
    Invalid(String),
    #[error("Incomplete request")]
    Incomplete,
    #[error("Request too large")]
    TooLarge,
    #[error("Invalid method: {0}")]
    InvalidMethod(String),
    #[error("Invalid URI: {0}")]
    InvalidUri(String),
    #[error("Invalid version: {0}")]
    InvalidVersion(String),
    #[error("Invalid header: {0}")]
    InvalidHeader(String),
    #[error("UTF-8 conversion error: {0}")]
    Utf8Error(#[from] std::str::Utf8Error),
}

/// HTTP methods
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Head,
    Options,
    Patch,
    Trace,
    Connect,
}

impl FromStr for HttpMethod {
    type Err = HttpParseError;
    
    fn from_str(s: &str) -> Result<Self, Self::Err> {
        match s.to_uppercase().as_str() {
            "GET" => Ok(HttpMethod::Get),
            "POST" => Ok(HttpMethod::Post),
            "PUT" => Ok(HttpMethod::Put),
            "DELETE" => Ok(HttpMethod::Delete),
            "HEAD" => Ok(HttpMethod::Head),
            "OPTIONS" => Ok(HttpMethod::Options),
            "PATCH" => Ok(HttpMethod::Patch),
            "TRACE" => Ok(HttpMethod::Trace),
            "CONNECT" => Ok(HttpMethod::Connect),
            _ => Err(HttpParseError::InvalidMethod(s.to_string())),
        }
    }
}

impl fmt::Display for HttpMethod {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            HttpMethod::Get => write!(f, "GET"),
            HttpMethod::Post => write!(f, "POST"),
            HttpMethod::Put => write!(f, "PUT"),
            HttpMethod::Delete => write!(f, "DELETE"),
            HttpMethod::Head => write!(f, "HEAD"),
            HttpMethod::Options => write!(f, "OPTIONS"),
            HttpMethod::Patch => write!(f, "PATCH"),
            HttpMethod::Trace => write!(f, "TRACE"),
            HttpMethod::Connect => write!(f, "CONNECT"),
        }
    }
}

impl Default for HttpRequest {
    fn default() -> Self {
        Self {
            method: "GET".to_string(),
            uri: "/".to_string(),
            version: "HTTP/1.1".to_string(),
            headers: HashMap::new(),
            body: Vec::new(),
        }
    }
}

impl HttpRequest {
    /// Parse HTTP request from bytes
    pub fn parse(data: &[u8]) -> Result<Self, HttpParseError> {
        let request_str = std::str::from_utf8(data)?;
        
        // Find the end of headers (double CRLF)
        let header_end = request_str.find("\r\n\r\n")
            .ok_or(HttpParseError::Incomplete)?;
        
        let header_section = &request_str[..header_end];
        let body_start = header_end + 4;
        
        // Parse request line
        let lines: Vec<&str> = header_section.split("\r\n").collect();
        if lines.is_empty() {
            return Err(HttpParseError::Invalid("Empty request".to_string()));
        }
        
        let request_line = lines[0];
        let parts: Vec<&str> = request_line.split_whitespace().collect();
        
        if parts.len() != 3 {
            return Err(HttpParseError::Invalid("Invalid request line".to_string()));
        }
        
        let method = parts[0].to_string();
        let uri = parts[1].to_string();
        let version = parts[2].to_string();
        
        // Validate method
        let _ = HttpMethod::from_str(&method)?;
        
        // Validate HTTP version
        if !version.starts_with("HTTP/") {
            return Err(HttpParseError::InvalidVersion(version.clone()));
        }
        
        // Parse headers
        let mut headers = HashMap::new();
        for line in &lines[1..] {
            if line.is_empty() {
                break;
            }
            
            let colon_pos = line.find(':')
                .ok_or_else(|| HttpParseError::InvalidHeader(line.to_string()))?;
            
            let name = line[..colon_pos].trim().to_lowercase();
            let value = line[colon_pos + 1..].trim().to_string();
            
            headers.insert(name, value);
        }
        
        // Parse body
        let body = if body_start < data.len() {
            data[body_start..].to_vec()
        } else {
            Vec::new()
        };
        
        Ok(HttpRequest {
            method,
            uri,
            version,
            headers,
            body,
        })
    }
    
    /// Get header value by name (case-insensitive)
    pub fn get_header(&self, name: &str) -> Option<&String> {
        self.headers.get(&name.to_lowercase())
    }
    
    /// Set header value
    pub fn set_header(&mut self, name: String, value: String) {
        self.headers.insert(name.to_lowercase(), value);
    }
    
    /// Get content length from headers
    pub fn get_content_length(&self) -> Option<usize> {
        self.get_header("content-length")
            .and_then(|v| v.parse().ok())
    }
    
    /// Get content type from headers
    pub fn get_content_type(&self) -> Option<&String> {
        self.get_header("content-type")
    }
    
    /// Get host from headers
    pub fn get_host(&self) -> Option<&String> {
        self.get_header("host")
    }
    
    /// Get user agent from headers
    pub fn get_user_agent(&self) -> Option<&String> {
        self.get_header("user-agent")
    }
    
    /// Get referer from headers
    pub fn get_referer(&self) -> Option<&String> {
        self.get_header("referer")
    }
    
    /// Get accept encoding from headers
    pub fn get_accept_encoding(&self) -> Option<&String> {
        self.get_header("accept-encoding")
    }
    
    /// Get If-None-Match header for cache validation
    pub fn get_if_none_match(&self) -> Option<&String> {
        self.get_header("if-none-match")
    }
    
    /// Get If-Modified-Since header for cache validation
    pub fn get_if_modified_since(&self) -> Option<&String> {
        self.get_header("if-modified-since")
    }
    
    /// Check if connection should be kept alive
    pub fn is_keep_alive(&self) -> bool {
        match self.get_header("connection") {
            Some(value) => value.to_lowercase() == "keep-alive",
            None => self.version == "HTTP/1.1", // HTTP/1.1 defaults to keep-alive
        }
    }
    
    /// Get parsed method as enum
    pub fn get_method(&self) -> Result<HttpMethod, HttpParseError> {
        HttpMethod::from_str(&self.method)
    }
}

impl HttpResponse {
    /// Create a new HTTP response
    pub fn new(status_code: u16, reason_phrase: &str) -> Self {
        Self {
            status_code,
            reason_phrase: reason_phrase.to_string(),
            version: "HTTP/1.1".to_string(),
            headers: HashMap::new(),
            body: Vec::new(),
        }
    }
    
    /// Set header value
    pub fn set_header(&mut self, name: String, value: String) {
        self.headers.insert(name.to_lowercase(), value);
    }
    
    /// Set body content
    pub fn set_body(&mut self, body: Vec<u8>) {
        let body_len = body.len();
        self.body = body;
        self.set_header("content-length".to_string(), body_len.to_string());
    }
    
    /// Convert response to bytes
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut response = Vec::new();
        
        // Status line
        let status_line = format!("{} {} {}\r\n", self.version, self.status_code, self.reason_phrase);
        response.extend_from_slice(status_line.as_bytes());
        
        // Headers
        for (name, value) in &self.headers {
            let header_line = format!("{}: {}\r\n", name, value);
            response.extend_from_slice(header_line.as_bytes());
        }
        
        // Empty line
        response.extend_from_slice(b"\r\n");
        
        // Body
        response.extend_from_slice(&self.body);
        
        response
    }
}

/// HTTP parser utility functions
pub mod utils {
    use super::*;
    
    /// Parse HTTP date format (RFC 7231)
    pub fn parse_http_date(date_str: &str) -> Option<std::time::SystemTime> {
        // Simplified implementation - in production use a proper HTTP date parser
        // This is a basic implementation for common formats
        use std::time::{SystemTime, UNIX_EPOCH, Duration};
        
        // Try to parse as Unix timestamp (for simplicity)
        if let Ok(timestamp) = date_str.parse::<u64>() {
            return Some(UNIX_EPOCH + Duration::from_secs(timestamp));
        }
        
        // TODO: Implement proper RFC 7231 date parsing
        None
    }
    
    /// Format HTTP date (RFC 7231)
    pub fn format_http_date(time: std::time::SystemTime) -> String {
        use std::time::UNIX_EPOCH;
        
        match time.duration_since(UNIX_EPOCH) {
            Ok(duration) => {
                // Simplified - in production use proper HTTP date formatting
                format!("{}", duration.as_secs())
            }
            Err(_) => "0".to_string(),
        }
    }
    
    /// URL decode a string
    pub fn url_decode(s: &str) -> Result<String, HttpParseError> {
        let mut result = String::new();
        let mut chars = s.chars().peekable();
        
        while let Some(ch) = chars.next() {
            match ch {
                '%' => {
                    if let (Some(h1), Some(h2)) = (chars.next(), chars.next()) {
                        let hex_str = format!("{}{}", h1, h2);
                        if let Ok(byte) = u8::from_str_radix(&hex_str, 16) {
                            result.push(byte as char);
                        } else {
                            return Err(HttpParseError::Invalid("Invalid URL encoding".to_string()));
                        }
                    } else {
                        return Err(HttpParseError::Invalid("Incomplete URL encoding".to_string()));
                    }
                }
                '+' => result.push(' '),
                _ => result.push(ch),
            }
        }
        
        Ok(result)
    }
    
    /// URL encode a string
    pub fn url_encode(s: &str) -> String {
        let mut result = String::new();
        
        for byte in s.bytes() {
            match byte {
                b'A'..=b'Z' | b'a'..=b'z' | b'0'..=b'9' | b'-' | b'_' | b'.' | b'~' => {
                    result.push(byte as char);
                }
                b' ' => result.push('+'),
                _ => {
                    result.push_str(&format!("%{:02X}", byte));
                }
            }
        }
        
        result
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    
    #[test]
    fn test_parse_simple_get_request() {
        let request = b"GET /index.html HTTP/1.1\r\nHost: example.com\r\nUser-Agent: ANX/1.0\r\n\r\n";
        
        let parsed = HttpRequest::parse(request).unwrap();
        assert_eq!(parsed.method, "GET");
        assert_eq!(parsed.uri, "/index.html");
        assert_eq!(parsed.version, "HTTP/1.1");
        assert_eq!(parsed.get_header("host").unwrap(), "example.com");
        assert_eq!(parsed.get_header("user-agent").unwrap(), "ANX/1.0");
    }
    
    #[test]
    fn test_parse_post_request_with_body() {
        let request = b"POST /api/data HTTP/1.1\r\nHost: example.com\r\nContent-Type: application/json\r\nContent-Length: 13\r\n\r\n{\"key\":\"value\"}";
        
        let parsed = HttpRequest::parse(request).unwrap();
        assert_eq!(parsed.method, "POST");
        assert_eq!(parsed.uri, "/api/data");
        assert_eq!(parsed.get_content_length().unwrap(), 13);
        assert_eq!(parsed.body, b"{\"key\":\"value\"}");
    }
    
    #[test]
    fn test_invalid_request() {
        let request = b"INVALID REQUEST";
        
        assert!(HttpRequest::parse(request).is_err());
    }
    
    #[test]
    fn test_http_response_creation() {
        let mut response = HttpResponse::new(200, "OK");
        response.set_header("content-type".to_string(), "text/html".to_string());
        response.set_body(b"<html></html>".to_vec());
        
        let bytes = response.to_bytes();
        let response_str = String::from_utf8(bytes).unwrap();
        
        assert!(response_str.contains("HTTP/1.1 200 OK"));
        assert!(response_str.contains("content-type: text/html"));
        assert!(response_str.contains("content-length: 13"));
        assert!(response_str.contains("<html></html>"));
    }
    
    #[test]
    fn test_url_decode() {
        assert_eq!(utils::url_decode("hello%20world").unwrap(), "hello world");
        assert_eq!(utils::url_decode("hello+world").unwrap(), "hello world");
        assert_eq!(utils::url_decode("hello%2Bworld").unwrap(), "hello+world");
    }
    
    #[test]
    fn test_url_encode() {
        assert_eq!(utils::url_encode("hello world"), "hello+world");
        assert_eq!(utils::url_encode("hello+world"), "hello%2Bworld");
    }
} 