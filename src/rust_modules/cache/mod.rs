//! Cache module for ANX HTTP Server
//! 
//! This module provides thread-safe caching functionality with HTTP-aware features.

use std::sync::{Arc, Mutex};
use std::collections::HashMap;
use std::time::{SystemTime, Duration};

#[derive(Debug, Clone)]
pub struct CacheEntry {
    pub data: Vec<u8>,
    pub content_type: String,
    pub etag: String,
    pub last_modified: SystemTime,
    pub ttl: Duration,
    pub created_at: SystemTime,
}

#[derive(Debug)]
struct CacheInner {
    store: HashMap<String, CacheEntry>,
    lru_list: Vec<String>,
    current_size: usize,
    max_size: usize,
    max_entries: usize,
    min_file_size: usize,
    max_file_size: usize,
    default_ttl: Duration,
    enabled: bool,
    hits: usize,
    misses: usize,
    evictions: usize,
}

impl CacheInner {
    fn new(max_size: usize, max_entries: usize, min_file_size: usize, max_file_size: usize, default_ttl: Duration) -> Self {
        CacheInner {
            store: HashMap::new(),
            lru_list: Vec::new(),
            current_size: 0,
            max_size,
            max_entries,
            min_file_size,
            max_file_size,
            default_ttl,
            enabled: true,
            hits: 0,
            misses: 0,
            evictions: 0,
        }
    }

    fn update_access_stats(&mut self, key: &str) {
        if let Some(pos) = self.lru_list.iter().position(|k| k == key) {
            self.lru_list.remove(pos);
        }
        self.lru_list.push(key.to_string());
    }

    fn is_cacheable_type(&self, content_type: &str) -> bool {
        let cacheable_types = [
            "text/", "application/json", "application/javascript",
            "application/xml", "image/", "font/", "application/x-font"
        ];
        cacheable_types.iter().any(|t| content_type.starts_with(t))
    }

    fn select_eviction_candidate(&mut self) -> Option<String> {
        self.lru_list.first().map(|s| s.to_string())
    }

    fn remove_from_lru(&mut self, key: &str) {
        if let Some(pos) = self.lru_list.iter().position(|k| k == key) {
            self.lru_list.remove(pos);
        }
    }

    fn add_to_lru_head(&mut self, key: &str) {
        self.lru_list.push(key.to_string());
    }
}

#[derive(Debug)]
pub struct Cache {
    inner: Arc<Mutex<CacheInner>>,
}

impl Cache {
    pub fn new(max_size: usize, max_entries: usize, min_file_size: usize, max_file_size: usize, default_ttl: Duration) -> Self {
        Cache {
            inner: Arc::new(Mutex::new(CacheInner::new(
                max_size,
                max_entries,
                min_file_size,
                max_file_size,
                default_ttl
            ))),
        }
    }

    pub fn get(&self, key: &str) -> Option<CacheEntry> {
        let mut inner = self.inner.lock().unwrap();
        if !inner.enabled {
            return None;
        }

        // 检查是否存在且未过期
        if let Some(entry) = inner.store.get(key) {
            if entry.created_at + entry.ttl > SystemTime::now() {
                inner.hits += 1;
                inner.update_access_stats(key);
                return Some(entry.clone());
            }
        }

        inner.misses += 1;
        None
    }

    pub fn put(&self, key: String, data: Vec<u8>, content_type: String, ttl: Option<Duration>) -> bool {
        let mut inner = self.inner.lock().unwrap();
        if !inner.enabled {
            return false;
        }

        // 检查文件大小限制
        if !data.is_empty() && (data.len() < inner.min_file_size || data.len() > inner.max_file_size) {
            return false;
        }

        // 检查内容类型
        if !inner.is_cacheable_type(&content_type) {
            return false;
        }

        // 如果需要，进行缓存清理
        while (inner.store.len() >= inner.max_entries || inner.current_size + data.len() > inner.max_size) 
            && !inner.store.is_empty() {
            if let Some(evict_key) = inner.select_eviction_candidate() {
                if let Some(entry) = inner.store.remove(&evict_key) {
                    inner.current_size -= entry.data.len();
                    inner.remove_from_lru(&evict_key);
                    inner.evictions += 1;
                }
            }
        }

        // 如果仍然超出限制，拒绝缓存
        if inner.store.len() >= inner.max_entries || inner.current_size + data.len() > inner.max_size {
            return false;
        }

        // 创建缓存条目
        let entry = CacheEntry {
            data: data.clone(),
            content_type,
            etag: format!("\"{:x}\"", md5::compute(&data)),
            last_modified: SystemTime::now(),
            ttl: ttl.unwrap_or(inner.default_ttl),
            created_at: SystemTime::now(),
        };

        // 更新缓存
        inner.current_size += data.len();
        inner.store.insert(key.clone(), entry);
        inner.add_to_lru_head(&key);

        true
    }

    pub fn remove(&self, key: &str) -> Option<CacheEntry> {
        let mut inner = self.inner.lock().unwrap();
        if let Some(entry) = inner.store.remove(key) {
            inner.current_size -= entry.data.len();
            inner.remove_from_lru(key);
            Some(entry)
        } else {
            None
        }
    }

    pub fn clear(&self) {
        let mut inner = self.inner.lock().unwrap();
        inner.store.clear();
        inner.lru_list.clear();
        inner.current_size = 0;
        inner.hits = 0;
        inner.misses = 0;
        inner.evictions = 0;
    }

    pub fn cleanup_expired(&self) {
        let mut inner = self.inner.lock().unwrap();
        let now = SystemTime::now();
        let expired_keys: Vec<String> = inner.store
            .iter()
            .filter(|(_, entry)| entry.created_at + entry.ttl <= now)
            .map(|(key, _)| key.clone())
            .collect();

        for key in expired_keys {
            if let Some(entry) = inner.store.remove(&key) {
                inner.current_size -= entry.data.len();
                inner.remove_from_lru(&key);
            }
        }
    }

    pub fn get_stats(&self) -> (usize, usize, usize, usize, usize) {
        let inner = self.inner.lock().unwrap();
        (
            inner.store.len(),
            inner.current_size,
            inner.hits,
            inner.misses,
            inner.evictions
        )
    }

    pub fn set_enabled(&self, enabled: bool) {
        let mut inner = self.inner.lock().unwrap();
        inner.enabled = enabled;
    }
} 