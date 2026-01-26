cmake_minimum_required(VERSION 3.14)
project(OrderBookEngine)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source Files
set (SOURCES 
    src/order.cpp)

add_executable(engine src/main.cpp ${SOURCES})

# Include directories
target_include_directories(engine PRIVATE include)


# Tests ( GoogleTest later)