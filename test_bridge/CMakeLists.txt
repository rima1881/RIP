cmake_minimum_required(VERSION 3.21)
project(test_bridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)

# Reuse the bridge library source directly (single shared library).
add_library(bridge_uart SHARED ../bridge/src/bridge_serial.cpp)

target_include_directories(bridge_uart PRIVATE
    ../bridge/include
    ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(bridge_uart PRIVATE Boost::headers pthread)

# Test executable that exercises the handshake + step loop against the ESP32.
add_executable(bridge_test src/main.cpp)

target_include_directories(bridge_test PRIVATE
    ../bridge/include)

target_link_libraries(bridge_test PRIVATE
    bridge_uart
    Boost::headers
    pthread)
