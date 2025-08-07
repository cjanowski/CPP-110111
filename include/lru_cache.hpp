#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <type_traits>
#include <unordered_map>

namespace interview {

struct CacheMetrics {
    size_t hits = 0;
    size_t misses = 0;
    double hit_ratio = 0.0;
    double average_access_time_ns = 0.0;
    size_t current_size = 0;
    size_t capacity = 0;
};

class CacheException : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

template<typename Key, typename Value, 
         typename Hash = std::hash<Key>,
         typename KeyEqual = std::equal_to<Key>>
class ThreadSafeLRUCache {
    static_assert(std::is_copy_constructible_v<Key>, "Key must be copy constructible");
    static_assert(std::is_move_constructible_v<Value>, "Value must be move constructible");

private:
    struct CacheEntry {
        Key key;
        Value value;
        std::chrono::steady_clock::time_point access_time;
        
        CacheEntry(Key k, Value v) 
            : key(std::move(k)), value(std::move(v)), 
              access_time(std::chrono::steady_clock::now()) {}
    };

    using EntryList = std::list<CacheEntry>;
    using EntryIterator = typename EntryList::iterator;
    using IndexMap = std::unordered_map<Key, EntryIterator, Hash, KeyEqual>;

public:
    explicit ThreadSafeLRUCache(size_t capacity);
    ~ThreadSafeLRUCache() = default;
    
    ThreadSafeLRUCache(const ThreadSafeLRUCache&) = delete;
    ThreadSafeLRUCache& operator=(const ThreadSafeLRUCache&) = delete;
    ThreadSafeLRUCache(ThreadSafeLRUCache&&) noexcept = default;
    ThreadSafeLRUCache& operator=(ThreadSafeLRUCache&&) noexcept = default;

    auto get(const Key& key) -> std::optional<Value>;
    auto put(Key key, Value value) -> bool;
    auto remove(const Key& key) -> bool;
    auto contains(const Key& key) const -> bool;
    
    auto size() const noexcept -> size_t;
    auto capacity() const noexcept -> size_t;
    auto empty() const noexcept -> bool;
    void clear() noexcept;
    
    auto get_metrics() const -> CacheMetrics;
    void reset_metrics() noexcept;

private:
    const size_t capacity_;
    EntryList lru_list_;
    IndexMap index_;
    
    mutable std::shared_mutex cache_mutex_;
    
    mutable std::atomic<size_t> hit_count_{0};
    mutable std::atomic<size_t> miss_count_{0};
    mutable std::atomic<uint64_t> total_access_time_ns_{0};
    
    void move_to_front_unsafe(EntryIterator it);
    void evict_if_needed_unsafe();
    auto record_access_time() const -> std::chrono::steady_clock::time_point;
    void update_access_metrics(std::chrono::steady_clock::time_point start_time) const;
};

template<typename Key, typename Value, typename Hash, typename KeyEqual>
ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::ThreadSafeLRUCache(size_t capacity) 
    : capacity_(capacity) {
    if (capacity == 0) {
        throw CacheException("Cache capacity must be greater than 0");
    }
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::get(const Key& key) -> std::optional<Value> {
    auto start_time = record_access_time();
    std::unique_lock lock(cache_mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) {
        miss_count_.fetch_add(1, std::memory_order_relaxed);
        update_access_metrics(start_time);
        return std::nullopt;
    }
    
    hit_count_.fetch_add(1, std::memory_order_relaxed);
    move_to_front_unsafe(it->second);
    
    Value result = it->second->value;
    update_access_metrics(start_time);
    return result;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::put(Key key, Value value) -> bool {
    auto start_time = record_access_time();
    std::unique_lock lock(cache_mutex_);
    
    auto it = index_.find(key);
    if (it != index_.end()) {
        it->second->value = std::move(value);
        move_to_front_unsafe(it->second);
        update_access_metrics(start_time);
        return true;
    }
    
    evict_if_needed_unsafe();
    
    lru_list_.emplace_front(std::move(key), std::move(value));
    index_[lru_list_.front().key] = lru_list_.begin();
    
    update_access_metrics(start_time);
    return true;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::remove(const Key& key) -> bool {
    std::unique_lock lock(cache_mutex_);
    
    auto it = index_.find(key);
    if (it == index_.end()) {
        return false;
    }
    
    lru_list_.erase(it->second);
    index_.erase(it);
    return true;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::contains(const Key& key) const -> bool {
    std::shared_lock lock(cache_mutex_);
    return index_.find(key) != index_.end();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::size() const noexcept -> size_t {
    std::shared_lock lock(cache_mutex_);
    return lru_list_.size();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::capacity() const noexcept -> size_t {
    return capacity_;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::empty() const noexcept -> bool {
    std::shared_lock lock(cache_mutex_);
    return lru_list_.empty();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::clear() noexcept {
    std::unique_lock lock(cache_mutex_);
    lru_list_.clear();
    index_.clear();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::get_metrics() const -> CacheMetrics {
    std::shared_lock lock(cache_mutex_);
    
    const size_t hits = hit_count_.load(std::memory_order_relaxed);
    const size_t misses = miss_count_.load(std::memory_order_relaxed);
    const size_t total_requests = hits + misses;
    
    CacheMetrics metrics;
    metrics.hits = hits;
    metrics.misses = misses;
    metrics.hit_ratio = total_requests > 0 ? static_cast<double>(hits) / total_requests : 0.0;
    metrics.current_size = lru_list_.size();
    metrics.capacity = capacity_;
    
    const uint64_t total_time_ns = total_access_time_ns_.load(std::memory_order_relaxed);
    metrics.average_access_time_ns = total_requests > 0 ? 
        static_cast<double>(total_time_ns) / total_requests : 0.0;
    
    return metrics;
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::reset_metrics() noexcept {
    hit_count_.store(0, std::memory_order_relaxed);
    miss_count_.store(0, std::memory_order_relaxed);
    total_access_time_ns_.store(0, std::memory_order_relaxed);
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::move_to_front_unsafe(EntryIterator it) {
    if (it != lru_list_.begin()) {
        lru_list_.splice(lru_list_.begin(), lru_list_, it);
    }
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::evict_if_needed_unsafe() {
    while (lru_list_.size() >= capacity_) {
        auto last = std::prev(lru_list_.end());
        index_.erase(last->key);
        lru_list_.erase(last);
    }
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
auto ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::record_access_time() const -> std::chrono::steady_clock::time_point {
    return std::chrono::steady_clock::now();
}

template<typename Key, typename Value, typename Hash, typename KeyEqual>
void ThreadSafeLRUCache<Key, Value, Hash, KeyEqual>::update_access_metrics(
    std::chrono::steady_clock::time_point start_time) const {
    auto end_time = std::chrono::steady_clock::now();
    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        end_time - start_time).count();
    total_access_time_ns_.fetch_add(static_cast<uint64_t>(duration_ns), std::memory_order_relaxed);
}

}  // namespace interview