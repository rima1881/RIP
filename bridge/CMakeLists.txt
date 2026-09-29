cmake_minimum_required(VERSION 3.21)
project(Bridge CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost REQUIRED)

# Lib
add_library(bridge_uart SHARED src/bridge_uart.cpp)

target_include_directories(bridge_uart PRIVATE include ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(bridge_uart PRIVATE Boost::headers pthread)
