#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <random>

#include "lru_cache.hpp"

using namespace interview;

void demonstrate_basic_usage() {
    std::cout << "=== Basic LRU Cache Demo ===\n";
    
    ThreadSafeLRUCache<int, std::string> cache(3);
    
    std::cout << "Cache capacity: " << cache.capacity() << "\n";
    std::cout << "Initial size: " << cache.size() << "\n";
    
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    
    std::cout << "Size after adding 3 items: " << cache.size() << "\n";
    
    auto value = cache.get(2);
    if (value) {
        std::cout << "Retrieved key 2: " << *value << "\n";
    }
    
    cache.put(4, "four");  // Should evict key 1 (least recently used)
    
    std::cout << "Size after adding 4th item: " << cache.size() << "\n";
    std::cout << "Key 1 exists: " << (cache.contains(1) ? "yes" : "no") << "\n";
    std::cout << "Key 2 exists: " << (cache.contains(2) ? "yes" : "no") << "\n";
    
    auto metrics = cache.get_metrics();
    std::cout << "Hit ratio: " << (metrics.hit_ratio * 100) << "%\n";
    std::cout << "Hits: " << metrics.hits << ", Misses: " << metrics.misses << "\n";
}

void demonstrate_thread_safety() {
    std::cout << "\n=== Thread Safety Demo ===\n";
    
    ThreadSafeLRUCache<int, int> cache(1000);
    constexpr int num_threads = 4;
    constexpr int operations_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    auto worker = [&cache]([[maybe_unused]] int thread_id, int start_key) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> key_dist(start_key, start_key + 100);
        std::uniform_int_distribution<> op_dist(0, 2);
        
        for (int i = 0; i < operations_per_thread; ++i) {
            int key = key_dist(gen);
            int operation = op_dist(gen);
            
            switch (operation) {
                case 0:  // put
                    cache.put(key, key * 10);
                    break;
                case 1:  // get
                    cache.get(key);
                    break;
                case 2:  // contains
                    cache.contains(key);
                    break;
            }
        }
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker, i, i * 200);
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time);
    
    auto metrics = cache.get_metrics();
    std::cout << "Concurrent operations completed in " << duration.count() << "ms\n";
    std::cout << "Final cache size: " << cache.size() << "\n";
    std::cout << "Total operations: " << (metrics.hits + metrics.misses) << "\n";
    std::cout << "Hit ratio: " << (metrics.hit_ratio * 100) << "%\n";
    std::cout << "Average access time: " << metrics.average_access_time_ns << "ns\n";
}

void demonstrate_performance() {
    std::cout << "\n=== Performance Demo ===\n";
    
    ThreadSafeLRUCache<int, std::string> cache(10000);
    constexpr int num_operations = 100000;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> key_dist(1, 50000);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < num_operations; ++i) {
        int key = key_dist(gen);
        
        if (i % 3 == 0) {
            cache.put(key, "value_" + std::to_string(key));
        } else {
            cache.get(key);
        }
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
        end_time - start_time);
    
    auto metrics = cache.get_metrics();
    std::cout << "Operations completed in " << duration.count() << "μs\n";
    std::cout << "Average time per operation: " 
              << (static_cast<double>(duration.count()) / num_operations) << "μs\n";
    std::cout << "Hit ratio: " << (metrics.hit_ratio * 100) << "%\n";
    std::cout << "Cache utilization: " 
              << (static_cast<double>(metrics.current_size) / metrics.capacity * 100) << "%\n";
}

int main() {
    try {
        demonstrate_basic_usage();
        demonstrate_thread_safety();
        demonstrate_performance();
        
        std::cout << "\n=== Demo completed successfully ===\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}