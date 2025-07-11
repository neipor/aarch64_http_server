//! ANX HTTP Server - Core Rust modules
//! 
//! This library provides the core Rust modules for the ANX HTTP server,
//! including configuration parsing, HTTP request handling, and caching.

use std::ffi::CStr;
use std::os::raw::c_char;

// Module declarations
pub mod rust_modules {
    pub mod config;
pub mod http_parser;
pub mod cache;
pub mod cli;
pub mod ffi;
}

// Re-export for convenience
pub use rust_modules::config;
pub use rust_modules::http_parser;
pub use rust_modules::cache;
pub use rust_modules::ffi;

// Re-export commonly used types
pub use config::{AnxConfig, ServerConfig, LocationConfig};
pub use http_parser::HttpRequest;
pub use cache::Cache;

/// Initialize the Rust modules
#[no_mangle]
pub extern "C" fn anx_rust_init() -> i32 {
    // Initialize logging
    env_logger::init();
    log::info!("ANX Rust modules initialized");
    0
}

/// Cleanup the Rust modules
#[no_mangle]
pub extern "C" fn anx_rust_cleanup() {
    log::info!("ANX Rust modules cleanup");
}
