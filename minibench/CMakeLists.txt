cmake_minimum_required(VERSION 3.10)
project(minibench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(minibench minibench.cpp)

# Add this line to link minibench against colintrace
target_link_libraries(minibench PRIVATE colintrace)
