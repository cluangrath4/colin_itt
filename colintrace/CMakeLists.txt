cmake_minimum_required(VERSION 3.10)
project(colintrace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(colintrace SHARED colintrace.cpp)

target_include_directories(colintrace PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(colintrace PRIVATE pthread)
