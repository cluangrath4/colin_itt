cmake_minimum_required(VERSION 3.13)
project(ITT_Project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(colintrace)
add_subdirectory(minibench)
