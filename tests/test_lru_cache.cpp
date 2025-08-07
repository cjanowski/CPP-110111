#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <memory>

#include "lru_cache.hpp"

using namespace interview;

class LRUCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<ThreadSafeLRUCache<int, std::string>>(3);
    }

    std::unique_ptr<ThreadSafeLRUCache<int, std::string>> cache;
};

TEST_F(LRUCacheTest, ConstructorWithValidCapacity) {
    EXPECT_EQ(cache->capacity(), 3);
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
}

TEST_F(LRUCacheTest, ConstructorWithZeroCapacityThrows) {
    EXPECT_THROW({
        ThreadSafeLRUCache<int, std::string> invalid_cache(0);
    }, CacheException);
}

TEST_F(LRUCacheTest, BasicPutAndGet) {
    EXPECT_TRUE(cache->put(1, "one"));
    EXPECT_EQ(cache->size(), 1);
    EXPECT_FALSE(cache->empty());
    
    auto result = cache->get(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "one");
}

TEST_F(LRUCacheTest, GetNonExistentKey) {
    auto result = cache->get(999);
    EXPECT_FALSE(result.has_value());
}

TEST_F(LRUCacheTest, UpdateExistingKey) {
    cache->put(1, "one");
    cache->put(1, "updated_one");
    
    auto result = cache->get(1);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "updated_one");
    EXPECT_EQ(cache->size(), 1);
}

TEST_F(LRUCacheTest, LRUEvictionBehavior) {
    cache->put(1, "one");
    cache->put(2, "two");
    cache->put(3, "three");
    EXPECT_EQ(cache->size(), 3);
    
    cache->put(4, "four");
    EXPECT_EQ(cache->size(), 3);
    
    EXPECT_FALSE(cache->contains(1));
    EXPECT_TRUE(cache->contains(2));
    EXPECT_TRUE(cache->contains(3));
    EXPECT_TRUE(cache->contains(4));
}

TEST_F(LRUCacheTest, AccessUpdatesOrder) {
    cache->put(1, "one");
    cache->put(2, "two");
    cache->put(3, "three");
    
    cache->get(1);
    
    cache->put(4, "four");
    
    EXPECT_TRUE(cache->contains(1));
    EXPECT_FALSE(cache->contains(2));
    EXPECT_TRUE(cache->contains(3));
    EXPECT_TRUE(cache->contains(4));
}

TEST_F(LRUCacheTest, RemoveExistingKey) {
    cache->put(1, "one");
    cache->put(2, "two");
    
    EXPECT_TRUE(cache->remove(1));
    EXPECT_EQ(cache->size(), 1);
    EXPECT_FALSE(cache->contains(1));
    EXPECT_TRUE(cache->contains(2));
}

TEST_F(LRUCacheTest, RemoveNonExistentKey) {
    EXPECT_FALSE(cache->remove(999));
}

TEST_F(LRUCacheTest, ContainsMethod) {
    EXPECT_FALSE(cache->contains(1));
    
    cache->put(1, "one");
    EXPECT_TRUE(cache->contains(1));
    
    cache->remove(1);
    EXPECT_FALSE(cache->contains(1));
}

TEST_F(LRUCacheTest, ClearCache) {
    cache->put(1, "one");
    cache->put(2, "two");
    EXPECT_EQ(cache->size(), 2);
    
    cache->clear();
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
    EXPECT_FALSE(cache->contains(1));
    EXPECT_FALSE(cache->contains(2));
}

TEST_F(LRUCacheTest, MetricsTracking) {
    cache->put(1, "one");
    cache->put(2, "two");
    
    cache->get(1);    // hit
    cache->get(2);    // hit
    cache->get(999);  // miss
    
    auto metrics = cache->get_metrics();
    EXPECT_EQ(metrics.hits, 2);
    EXPECT_EQ(metrics.misses, 1);
    EXPECT_DOUBLE_EQ(metrics.hit_ratio, 2.0 / 3.0);
    EXPECT_EQ(metrics.current_size, 2);
    EXPECT_EQ(metrics.capacity, 3);
    EXPECT_GT(metrics.average_access_time_ns, 0);
}

TEST_F(LRUCacheTest, ResetMetrics) {
    cache->put(1, "one");
    cache->get(1);
    cache->get(999);
    
    cache->reset_metrics();
    
    auto metrics = cache->get_metrics();
    EXPECT_EQ(metrics.hits, 0);
    EXPECT_EQ(metrics.misses, 0);
    EXPECT_EQ(metrics.hit_ratio, 0.0);
    EXPECT_EQ(metrics.average_access_time_ns, 0.0);
}

TEST_F(LRUCacheTest, MoveSemantics) {
    cache->put(1, "one");
    cache->put(2, "two");
    
    auto moved_cache = std::move(*cache);
    EXPECT_EQ(moved_cache.size(), 2);
    EXPECT_TRUE(moved_cache.contains(1));
    EXPECT_TRUE(moved_cache.contains(2));
}

TEST_F(LRUCacheTest, LargeCapacity) {
    ThreadSafeLRUCache<int, int> large_cache(10000);
    
    for (int i = 0; i < 5000; ++i) {
        large_cache.put(i, i * 2);
    }
    
    EXPECT_EQ(large_cache.size(), 5000);
    
    for (int i = 0; i < 5000; ++i) {
        auto result = large_cache.get(i);
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(*result, i * 2);
    }
}

TEST_F(LRUCacheTest, DifferentKeyTypes) {
    ThreadSafeLRUCache<std::string, int> string_cache(5);
    
    string_cache.put("hello", 1);
    string_cache.put("world", 2);
    
    auto result = string_cache.get("hello");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
    
    EXPECT_TRUE(string_cache.contains("world"));
    EXPECT_FALSE(string_cache.contains("foo"));
}

class CustomHash {
public:
    size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>{}(p.first) ^ (std::hash<int>{}(p.second) << 1);
    }
};

class CustomEqual {
public:
    bool operator()(const std::pair<int, int>& a, const std::pair<int, int>& b) const {
        return a.first == b.first && a.second == b.second;
    }
};

TEST_F(LRUCacheTest, CustomHashAndEqual) {
    ThreadSafeLRUCache<std::pair<int, int>, std::string, CustomHash, CustomEqual> 
        custom_cache(3);
    
    custom_cache.put({1, 2}, "one_two");
    custom_cache.put({3, 4}, "three_four");
    
    auto result = custom_cache.get({1, 2});
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "one_two");
    
    EXPECT_TRUE(custom_cache.contains({3, 4}));
    EXPECT_FALSE(custom_cache.contains({5, 6}));
}