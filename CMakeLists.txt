cmake_minimum_required(VERSION 3.20)
project(LRUCacheInterview CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra -Wpedantic -fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

include_directories(include)

add_library(lru_cache INTERFACE)
target_include_directories(lru_cache INTERFACE include)

add_executable(demo src/main.cpp)
target_link_libraries(demo lru_cache)

find_package(GTest QUIET)
if(GTest_FOUND)
    enable_testing()
    add_executable(tests 
        tests/test_lru_cache.cpp
        tests/test_thread_safety.cpp
    )
    target_link_libraries(tests lru_cache GTest::gtest GTest::gtest_main pthread)
    
    include(GoogleTest)
    gtest_discover_tests(tests)
else()
    message(WARNING "GTest not found. Tests will not be built.")
endif()