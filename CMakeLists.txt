# CMakeLists for WebSocket Module
#
# © 2024 by Hatem Nabli

cmake_minimum_required(VERSION 3.8)

set(this WebSocket)

set(Headers 
    include/WebSocket/WebSocket.hpp
)

set(Sources
    src/WebSocket.cpp
)

add_library(${this} STATIC ${Headers} ${Sources})
set_target_properties(${this} PROPERTIES
    FOLDER Libraries
)

target_include_directories(${this} PUBLIC include)

add_subdirectory(test)
 