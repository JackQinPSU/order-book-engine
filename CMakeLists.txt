cmake_minimum_required(VERSION 3.15)
project(OrderBookEngine)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG release-1.12.1
)
FetchContent_MakeAvailable(googletest)

# Main engine
add_executable(engine
    src/main.cpp
    src/order.cpp
)
target_include_directories(engine PRIVATE include) # Looking inside include for headers

# Test 
enable_testing()

add_executable(tests
    tests/test_order.cpp
    src/order.cpp
)
target_include_directories(tests PRIVATE include)
target_link_libraries(tests PRIVATE gtest_main)  # Glue googe test onto tests

add_test(NAME OrderTests COMMAND $<TARGET_FILE:tests>) # Run test.exe when running tests
