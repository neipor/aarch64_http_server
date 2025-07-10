//! Cache module for ANX HTTP Server
//! 
//! This module provides thread-safe caching functionality with HTTP-aware features.

use std::collections::HashMap;
use std::sync::{Arc, RwLock, Mutex};
use std::time::{Duration, Instant, SystemTime, UNIX_EPOCH};
use thiserror::Error;

/// Cache entry
#[derive(Debug, Clone)]
pub struct CacheEntry {
    pub key: String,
    pub data: Vec<u8>,
    pub content_type: Option<String>,
    pub etag: Option<String>,
    pub last_modified: Option<SystemTime>,
    pub expires: Option<SystemTime>,
    pub created_at: Instant,
    pub last_accessed: Instant,
    pub access_count: u64,
    pub ttl: Duration,
    pub is_compressed: bool,
}

/// Cache response for HTTP requests
#[derive(Debug, Clone)]
pub struct CacheResponse {
    pub data: Vec<u8>,
    pub content_type: Option<String>,
    pub etag: Option<String>,
    pub last_modified: Option<SystemTime>,
    pub is_compressed: bool,
    pub needs_validation: bool,
}

/// Cache errors
#[derive(Error, Debug)]
pub enum CacheError {
    #[error("Cache miss")]
    Miss,
    #[error("Cache full")]
    Full,
    #[error("Invalid key")]
    InvalidKey,
    #[error("Entry expired")]
    Expired,
    #[error("Invalid cache configuration")]
    InvalidConfig,
}

/// Cache strategy enumeration
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum CacheStrategy {
    LRU,  // Least Recently Used
    LFU,  // Least Frequently Used
    FIFO, // First In First Out
}

/// Cache configuration
#[derive(Debug, Clone)]
pub struct CacheConfig {
    pub enabled: bool,
    pub max_size: usize,
    pub max_entries: usize,
    pub default_ttl: Duration,
    pub strategy: CacheStrategy,
    pub cacheable_types: Vec<String>,
    pub min_file_size: usize,
    pub max_file_size: usize,
    pub enable_etag: bool,
    pub enable_last_modified: bool,
}

impl Default for CacheConfig {
    fn default() -> Self {
        Self {
            enabled: true,
            max_size: 64 * 1024 * 1024, // 64MB
            max_entries: 10000,
            default_ttl: Duration::from_secs(3600), // 1 hour
            strategy: CacheStrategy::LRU,
            cacheable_types: vec![
                "text/html".to_string(),
                "text/css".to_string(),
                "text/javascript".to_string(),
                "text/plain".to_string(),
                "application/javascript".to_string(),
                "application/json".to_string(),
                "image/jpeg".to_string(),
                "image/png".to_string(),
                "image/gif".to_string(),
                "image/webp".to_string(),
                "image/svg+xml".to_string(),
                "font/woff".to_string(),
                "font/woff2".to_string(),
            ],
            min_file_size: 1,         // 1 byte (allow small files for testing)
            max_file_size: 10 * 1024 * 1024, // 10MB
            enable_etag: true,
            enable_last_modified: true,
        }
    }
}

/// Cache statistics
#[derive(Debug, Default, Clone)]
pub struct CacheStats {
    pub hits: u64,
    pub misses: u64,
    pub puts: u64,
    pub evictions: u64,
    pub current_size: usize,
    pub current_entries: usize,
}

impl CacheStats {
    pub fn hit_rate(&self) -> f64 {
        if self.hits + self.misses == 0 {
            0.0
        } else {
            self.hits as f64 / (self.hits + self.misses) as f64
        }
    }
}

/// LRU node for double-linked list
#[derive(Debug)]
struct LruNode {
    key: String,
    prev: Option<String>,
    next: Option<String>,
}

/// Thread-safe cache implementation
#[derive(Debug)]
pub struct Cache {
    store: Arc<RwLock<HashMap<String, CacheEntry>>>,
    lru_list: Arc<Mutex<HashMap<String, LruNode>>>,
    head: Arc<Mutex<Option<String>>>,
    tail: Arc<Mutex<Option<String>>>,
    config: CacheConfig,
    stats: Arc<Mutex<CacheStats>>,
    current_size: Arc<Mutex<usize>>,
}

impl Cache {
    /// Create a new cache with default configuration
    pub fn new() -> Self {
        Self::with_config(CacheConfig::default())
    }
    
    /// Create a new cache with specified configuration
    pub fn with_config(config: CacheConfig) -> Self {
        Self {
            store: Arc::new(RwLock::new(HashMap::new())),
            lru_list: Arc::new(Mutex::new(HashMap::new())),
            head: Arc::new(Mutex::new(None)),
            tail: Arc::new(Mutex::new(None)),
            config,
            stats: Arc::new(Mutex::new(CacheStats::default())),
            current_size: Arc::new(Mutex::new(0)),
        }
    }
    
    /// Get value from cache
    pub fn get(&self, key: &str) -> Result<CacheResponse, CacheError> {
        if !self.config.enabled {
            return Err(CacheError::Miss);
        }
        
        let store = self.store.read().unwrap();
        
        if let Some(entry) = store.get(key) {
            // Check if entry is still valid
            if entry.created_at.elapsed() < entry.ttl {
                // Update access statistics
                self.update_access_stats(key);
                
                // Update stats
                {
                    let mut stats = self.stats.lock().unwrap();
                    stats.hits += 1;
                }
                
                Ok(CacheResponse {
                    data: entry.data.clone(),
                    content_type: entry.content_type.clone(),
                    etag: entry.etag.clone(),
                    last_modified: entry.last_modified,
                    is_compressed: entry.is_compressed,
                    needs_validation: false,
                })
            } else {
                // Entry expired
                {
                    let mut stats = self.stats.lock().unwrap();
                    stats.misses += 1;
                }
                Err(CacheError::Expired)
            }
        } else {
            {
                let mut stats = self.stats.lock().unwrap();
                stats.misses += 1;
            }
            Err(CacheError::Miss)
        }
    }
    
    /// Get value from cache with conditional request support
    pub fn get_conditional(&self, key: &str, if_none_match: Option<&str>, if_modified_since: Option<SystemTime>) -> Result<CacheResponse, CacheError> {
        if !self.config.enabled {
            return Err(CacheError::Miss);
        }
        
        let store = self.store.read().unwrap();
        
        if let Some(entry) = store.get(key) {
            // Check if entry is still valid
            if entry.created_at.elapsed() < entry.ttl {
                // Check conditional headers
                let mut needs_validation = false;
                
                // Check If-None-Match (ETag)
                if let (Some(etag), Some(client_etag)) = (&entry.etag, if_none_match) {
                    if etag == client_etag {
                        needs_validation = true;
                    }
                }
                
                // Check If-Modified-Since
                if let (Some(last_modified), Some(client_time)) = (entry.last_modified, if_modified_since) {
                    if last_modified <= client_time {
                        needs_validation = true;
                    }
                }
                
                // Update access statistics
                self.update_access_stats(key);
                
                // Update stats
                {
                    let mut stats = self.stats.lock().unwrap();
                    stats.hits += 1;
                }
                
                Ok(CacheResponse {
                    data: entry.data.clone(),
                    content_type: entry.content_type.clone(),
                    etag: entry.etag.clone(),
                    last_modified: entry.last_modified,
                    is_compressed: entry.is_compressed,
                    needs_validation,
                })
            } else {
                {
                    let mut stats = self.stats.lock().unwrap();
                    stats.misses += 1;
                }
                Err(CacheError::Expired)
            }
        } else {
            {
                let mut stats = self.stats.lock().unwrap();
                stats.misses += 1;
            }
            Err(CacheError::Miss)
        }
    }
    
    /// Put value into cache
    pub fn put(&self, key: String, data: Vec<u8>, content_type: Option<String>) -> Result<(), CacheError> {
        self.put_with_metadata(key, data, content_type, None, None, None)
    }
    
    /// Put value into cache with custom TTL
    pub fn put_with_ttl(&self, key: String, data: Vec<u8>, content_type: Option<String>, ttl: Duration) -> Result<(), CacheError> {
        self.put_with_metadata(key, data, content_type, None, None, Some(ttl))
    }
    
    /// Put value into cache with full metadata
    pub fn put_with_metadata(&self, key: String, data: Vec<u8>, content_type: Option<String>, etag: Option<String>, last_modified: Option<SystemTime>, ttl: Option<Duration>) -> Result<(), CacheError> {
        if !self.config.enabled {
            return Err(CacheError::InvalidConfig);
        }
        
        // Check if content type is cacheable (only if content_type is provided)
        if let Some(ref ct) = content_type {
            if !self.is_cacheable_type(ct) {
                return Err(CacheError::InvalidConfig);
            }
        }
        
        // Check file size constraints (only if data is not empty)
        if !data.is_empty() && (data.len() < self.config.min_file_size || data.len() > self.config.max_file_size) {
            return Err(CacheError::InvalidConfig);
        }
        
        let mut store = self.store.write().unwrap();
        let mut current_size = self.current_size.lock().unwrap();
        
        // Check if we need to evict entries
        while (store.len() >= self.config.max_entries || *current_size + data.len() > self.config.max_size) && !store.is_empty() {
            if let Some(evict_key) = self.select_eviction_candidate() {
                if let Some(evicted) = store.remove(&evict_key) {
                    *current_size -= evicted.data.len();
                    self.remove_from_lru(&evict_key);
                    
                    // Update stats
                    {
                        let mut stats = self.stats.lock().unwrap();
                        stats.evictions += 1;
                        stats.current_entries = store.len();
                    }
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        
        // Check if cache is still full
        if store.len() >= self.config.max_entries || *current_size + data.len() > self.config.max_size {
            return Err(CacheError::Full);
        }
        
        let now = Instant::now();
        let entry = CacheEntry {
            key: key.clone(),
            data: data.clone(),
            content_type,
            etag,
            last_modified,
            expires: None,
            created_at: now,
            last_accessed: now,
            access_count: 0,
            ttl: ttl.unwrap_or(self.config.default_ttl),
            is_compressed: false,
        };
        
        // Remove existing entry if present
        if let Some(existing) = store.remove(&key) {
            *current_size -= existing.data.len();
            self.remove_from_lru(&key);
        }
        
        // Add new entry
        *current_size += data.len();
        store.insert(key.clone(), entry);
        self.add_to_lru_head(&key);
        
        // Update stats
        {
            let mut stats = self.stats.lock().unwrap();
            stats.puts += 1;
            stats.current_size = *current_size;
            stats.current_entries = store.len();
        }
        
        Ok(())
    }
    
    /// Remove value from cache
    pub fn remove(&self, key: &str) -> Result<(), CacheError> {
        let mut store = self.store.write().unwrap();
        
        if let Some(entry) = store.remove(key) {
            let mut current_size = self.current_size.lock().unwrap();
            *current_size -= entry.data.len();
            self.remove_from_lru(key);
            
            // Update stats
            {
                let mut stats = self.stats.lock().unwrap();
                stats.current_size = *current_size;
                stats.current_entries = store.len();
            }
            
            Ok(())
        } else {
            Err(CacheError::Miss)
        }
    }
    
    /// Clear all entries from cache
    pub fn clear(&self) {
        let mut store = self.store.write().unwrap();
        store.clear();
        
        let mut current_size = self.current_size.lock().unwrap();
        *current_size = 0;
        
        // Clear LRU list
        {
            let mut lru_list = self.lru_list.lock().unwrap();
            lru_list.clear();
            
            let mut head = self.head.lock().unwrap();
            *head = None;
            
            let mut tail = self.tail.lock().unwrap();
            *tail = None;
        }
        
        // Reset stats
        {
            let mut stats = self.stats.lock().unwrap();
            stats.current_size = 0;
            stats.current_entries = 0;
        }
    }
    
    /// Get cache statistics
    pub fn get_stats(&self) -> CacheStats {
        self.stats.lock().unwrap().clone()
    }
    
    /// Clean up expired entries
    pub fn cleanup_expired(&self) {
        let mut store = self.store.write().unwrap();
        let mut current_size = self.current_size.lock().unwrap();
        let mut expired_keys = Vec::new();
        
        for (key, entry) in store.iter() {
            if entry.created_at.elapsed() >= entry.ttl {
                expired_keys.push(key.clone());
            }
        }
        
        for key in expired_keys {
            if let Some(entry) = store.remove(&key) {
                *current_size -= entry.data.len();
                self.remove_from_lru(&key);
            }
        }
        
        // Update stats
        {
            let mut stats = self.stats.lock().unwrap();
            stats.current_size = *current_size;
            stats.current_entries = store.len();
        }
    }
    
    /// Generate ETag for content
    pub fn generate_etag(content: &[u8], last_modified: Option<SystemTime>) -> String {
        use std::collections::hash_map::DefaultHasher;
        use std::hash::{Hash, Hasher};
        
        let mut hasher = DefaultHasher::new();
        content.hash(&mut hasher);
        
        if let Some(mtime) = last_modified {
            if let Ok(duration) = mtime.duration_since(UNIX_EPOCH) {
                duration.as_secs().hash(&mut hasher);
            }
        }
        
        format!("\"{}\"", hasher.finish())
    }
    
    // Private helper methods
    
    fn is_cacheable_type(&self, content_type: &str) -> bool {
        // Allow all types if no specific cacheable types are configured
        if self.config.cacheable_types.is_empty() {
            return true;
        }
        self.config.cacheable_types.iter().any(|ct| content_type.starts_with(ct))
    }
    
    fn update_access_stats(&self, key: &str) {
        // Update LRU order
        self.move_to_lru_head(key);
        
        // Update access count in entry
        let mut store = self.store.write().unwrap();
        if let Some(entry) = store.get_mut(key) {
            entry.last_accessed = Instant::now();
            entry.access_count += 1;
        }
    }
    
    fn select_eviction_candidate(&self) -> Option<String> {
        match self.config.strategy {
            CacheStrategy::LRU => {
                let tail = self.tail.lock().unwrap();
                tail.clone()
            }
            CacheStrategy::LFU => {
                let store = self.store.read().unwrap();
                store.iter()
                    .min_by_key(|(_, entry)| entry.access_count)
                    .map(|(key, _)| key.clone())
            }
            CacheStrategy::FIFO => {
                let store = self.store.read().unwrap();
                store.iter()
                    .min_by_key(|(_, entry)| entry.created_at)
                    .map(|(key, _)| key.clone())
            }
        }
    }
    
    fn add_to_lru_head(&self, key: &str) {
        let mut lru_list = self.lru_list.lock().unwrap();
        let mut head = self.head.lock().unwrap();
        let mut tail = self.tail.lock().unwrap();
        
        let node = LruNode {
            key: key.to_string(),
            prev: None,
            next: head.clone(),
        };
        
        if let Some(ref old_head) = *head {
            if let Some(old_head_node) = lru_list.get_mut(old_head) {
                old_head_node.prev = Some(key.to_string());
            }
        }
        
        lru_list.insert(key.to_string(), node);
        *head = Some(key.to_string());
        
        if tail.is_none() {
            *tail = Some(key.to_string());
        }
    }
    
    fn remove_from_lru(&self, key: &str) {
        let mut lru_list = self.lru_list.lock().unwrap();
        let mut head = self.head.lock().unwrap();
        let mut tail = self.tail.lock().unwrap();
        
        if let Some(node) = lru_list.remove(key) {
            if let Some(ref prev_key) = node.prev {
                if let Some(prev_node) = lru_list.get_mut(prev_key) {
                    prev_node.next = node.next.clone();
                }
            } else {
                *head = node.next.clone();
            }
            
            if let Some(ref next_key) = node.next {
                if let Some(next_node) = lru_list.get_mut(next_key) {
                    next_node.prev = node.prev.clone();
                }
            } else {
                *tail = node.prev.clone();
            }
        }
    }
    
    fn move_to_lru_head(&self, key: &str) {
        // Remove from current position
        self.remove_from_lru(key);
        // Add to head
        self.add_to_lru_head(key);
    }
}

impl Default for Cache {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::thread;
    use std::time::Duration;
    
    #[test]
    fn test_basic_cache_operations() {
        let cache = Cache::new();
        
        // Test put and get
        let key = "test_key".to_string();
        let data = b"test data".to_vec();
        let content_type = Some("text/plain".to_string());
        
        cache.put(key.clone(), data.clone(), content_type.clone()).unwrap();
        
        let response = cache.get(&key).unwrap();
        assert_eq!(response.data, data);
        assert_eq!(response.content_type, content_type);
    }
    
    #[test]
    fn test_cache_miss() {
        let cache = Cache::new();
        
        let result = cache.get("nonexistent");
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), CacheError::Miss));
    }
    
    #[test]
    fn test_cache_expiration() {
        let mut config = CacheConfig::default();
        config.default_ttl = Duration::from_millis(100);
        let cache = Cache::with_config(config);
        
        let key = "expire_test".to_string();
        let data = b"expire data".to_vec();
        
        cache.put(key.clone(), data, None).unwrap();
        
        // Should be available immediately
        assert!(cache.get(&key).is_ok());
        
        // Wait for expiration
        thread::sleep(Duration::from_millis(150));
        
        // Should be expired
        let result = cache.get(&key);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), CacheError::Expired));
    }
    
    #[test]
    fn test_conditional_cache_get() {
        let cache = Cache::new();
        
        let key = "conditional_test".to_string();
        let data = b"conditional data".to_vec();
        let etag = Some("\"12345\"".to_string());
        let last_modified = Some(SystemTime::now());
        
        cache.put_with_metadata(key.clone(), data, Some("text/plain".to_string()), etag.clone(), last_modified, None).unwrap();
        
        // Test ETag validation
        let response = cache.get_conditional(&key, Some("\"12345\""), None).unwrap();
        assert!(response.needs_validation);
        
        // Test without validation
        let response = cache.get_conditional(&key, Some("\"different\""), None).unwrap();
        assert!(!response.needs_validation);
    }
    
    #[test]
    fn test_cache_stats() {
        let cache = Cache::new();
        
        let key = "stats_test".to_string();
        let data = b"stats data".to_vec();
        
        cache.put(key.clone(), data, None).unwrap();
        cache.get(&key).unwrap();
        let _ = cache.get("nonexistent");
        
        let stats = cache.get_stats();
        assert_eq!(stats.puts, 1);
        assert_eq!(stats.hits, 1);
        assert_eq!(stats.misses, 1);
        assert_eq!(stats.current_entries, 1);
        assert!(stats.hit_rate() > 0.0);
    }
    
    #[test]
    fn test_etag_generation() {
        let data = b"test data";
        let etag = Cache::generate_etag(data, None);
        assert!(etag.starts_with('"'));
        assert!(etag.ends_with('"'));
        
        // Same data should generate same ETag
        let etag2 = Cache::generate_etag(data, None);
        assert_eq!(etag, etag2);
    }
    
    #[test]
    fn test_cache_clear() {
        let cache = Cache::new();
        
        cache.put("key1".to_string(), b"data1".to_vec(), None).unwrap();
        cache.put("key2".to_string(), b"data2".to_vec(), None).unwrap();
        
        let stats = cache.get_stats();
        assert_eq!(stats.current_entries, 2);
        
        cache.clear();
        
        let stats = cache.get_stats();
        assert_eq!(stats.current_entries, 0);
        assert_eq!(stats.current_size, 0);
    }
} 