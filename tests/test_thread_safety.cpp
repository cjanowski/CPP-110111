#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <chrono>
#include <barrier>

#include "lru_cache.hpp"

using namespace interview;

class ThreadSafetyTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = std::make_unique<ThreadSafeLRUCache<int, int>>(1000);
    }

    std::unique_ptr<ThreadSafeLRUCache<int, int>> cache;
};

TEST_F(ThreadSafetyTest, ConcurrentPuts) {
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 1000;
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread]() {
            int start_key = t * operations_per_thread;
            for (int i = 0; i < operations_per_thread; ++i) {
                cache->put(start_key + i, (start_key + i) * 2);
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(cache->size(), 1000);
    
    for (int t = 0; t < num_threads; ++t) {
        int start_key = t * operations_per_thread;
        for (int i = 0; i < operations_per_thread; ++i) {
            int key = start_key + i;
            if (cache->contains(key)) {
                auto value = cache->get(key);
                ASSERT_TRUE(value.has_value());
                EXPECT_EQ(*value, key * 2);
            }
        }
    }
}

TEST_F(ThreadSafetyTest, ConcurrentGets) {
    for (int i = 0; i < 100; ++i) {
        cache->put(i, i * 10);
    }
    
    constexpr int num_threads = 16;
    constexpr int gets_per_thread = 1000;
    std::atomic<int> successful_gets{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, gets_per_thread, &successful_gets]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(0, 99);
            
            for (int i = 0; i < gets_per_thread; ++i) {
                int key = dist(gen);
                auto value = cache->get(key);
                if (value.has_value() && *value == key * 10) {
                    successful_gets.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_GT(successful_gets.load(), 0);
}

TEST_F(ThreadSafetyTest, MixedOperations) {
    constexpr int num_threads = 12;
    constexpr int operations_per_thread = 500;
    std::atomic<int> put_count{0};
    std::atomic<int> get_count{0};
    std::atomic<int> remove_count{0};
    std::vector<std::thread> threads;
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, &put_count, &get_count, &remove_count]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(1, 2000);
            std::uniform_int_distribution<> op_dist(0, 9);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int operation = op_dist(gen);
                
                if (operation < 5) {  // 50% puts
                    cache->put(key, key * 100);
                    put_count.fetch_add(1, std::memory_order_relaxed);
                } else if (operation < 9) {  // 40% gets
                    cache->get(key);
                    get_count.fetch_add(1, std::memory_order_relaxed);
                } else {  // 10% removes
                    cache->remove(key);
                    remove_count.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(put_count.load(), num_threads * operations_per_thread * 0.5);
    EXPECT_EQ(get_count.load(), num_threads * operations_per_thread * 0.4);
    EXPECT_EQ(remove_count.load(), num_threads * operations_per_thread * 0.1);
    EXPECT_LE(cache->size(), cache->capacity());
}

TEST_F(ThreadSafetyTest, ConcurrentMetricsAccess) {
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<size_t> total_hits{0};
    std::atomic<size_t> total_misses{0};
    
    for (int i = 0; i < 50; ++i) {
        cache->put(i, i);
    }
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, operations_per_thread, &total_hits, &total_misses]() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> key_dist(0, 100);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                cache->get(key);
                
                if (i % 10 == 0) {
                    auto metrics = cache->get_metrics();
                    total_hits.fetch_add(metrics.hits, std::memory_order_relaxed);
                    total_misses.fetch_add(metrics.misses, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto final_metrics = cache->get_metrics();
    EXPECT_EQ(final_metrics.hits + final_metrics.misses, num_threads * operations_per_thread);
    EXPECT_GE(final_metrics.hit_ratio, 0.0);
    EXPECT_LE(final_metrics.hit_ratio, 1.0);
}

TEST_F(ThreadSafetyTest, StressTestWithBarrier) {
    constexpr int num_threads = 16;
    constexpr int operations_per_thread = 1000;
    std::vector<std::thread> threads;
    std::barrier sync_point(num_threads);
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread, &sync_point]() {
            sync_point.arrive_and_wait();
            
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> key_dist(1, 5000);
            std::uniform_int_distribution<> op_dist(0, 10);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                int operation = op_dist(gen);
                
                switch (operation % 4) {
                    case 0:
                        cache->put(key, key * 2);
                        break;
                    case 1:
                        cache->get(key);
                        break;
                    case 2:
                        cache->contains(key);
                        break;
                    case 3:
                        if (key % 100 == 0) {  // Occasional remove
                            cache->remove(key);
                        }
                        break;
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_LE(cache->size(), cache->capacity());
    
    auto metrics = cache->get_metrics();
    EXPECT_GT(metrics.hits + metrics.misses, 0);
    EXPECT_GE(metrics.hit_ratio, 0.0);
    EXPECT_LE(metrics.hit_ratio, 1.0);
}

TEST_F(ThreadSafetyTest, ConcurrentClear) {
    for (int i = 0; i < 100; ++i) {
        cache->put(i, i);
    }
    EXPECT_EQ(cache->size(), 100);
    
    constexpr int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, &start_flag]() {
            while (!start_flag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            cache->clear();
        });
    }
    
    start_flag.store(true, std::memory_order_release);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(cache->size(), 0);
    EXPECT_TRUE(cache->empty());
}

TEST_F(ThreadSafetyTest, PerformanceUnderContention) {
    constexpr int num_threads = 8;
    constexpr int operations_per_thread = 10000;
    std::vector<std::thread> threads;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, operations_per_thread]() {
            std::random_device rd;
            std::mt19937 gen(rd() + t);
            std::uniform_int_distribution<> key_dist(1, 10000);
            std::uniform_int_distribution<> op_dist(0, 2);
            
            for (int i = 0; i < operations_per_thread; ++i) {
                int key = key_dist(gen);
                
                if (op_dist(gen) == 0) {
                    cache->put(key, key * 3);
                } else {
                    cache->get(key);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    auto metrics = cache->get_metrics();
    const size_t total_operations = metrics.hits + metrics.misses;
    const double ops_per_second = static_cast<double>(total_operations) / 
                                  (duration.count() / 1000.0);
    
    EXPECT_GT(ops_per_second, 10000);  // Should handle at least 10k ops/sec
    EXPECT_LE(cache->size(), cache->capacity());
}