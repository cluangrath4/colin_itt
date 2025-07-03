cmake_minimum_required(VERSION 3.10)
project(minibench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(minibench minibench.cpp)

target_include_directories(minibench PRIVATE ${CMAKE_SOURCE_DIR}/include)

message(STATUS "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}")
message(STATUS "Library path: ${CMAKE_SOURCE_DIR}/lib/libittnotify.so")

add_library(ittnotify SHARED IMPORTED)
set_target_properties(ittnotify PROPERTIES
    IMPORTED_LOCATION "${CMAKE_SOURCE_DIR}/lib/libittnotify.so"
)

target_link_libraries(minibench PRIVATE ittnotify)
