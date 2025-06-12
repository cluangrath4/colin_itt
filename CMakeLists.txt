cmake_minimum_required(VERSION 3.10)
project(colintrace LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(colintrace SHARED colintrace.cpp)
target_compile_definitions(colintrace PRIVATE BUILD_WITH_ITT=1)
target_include_directories(colintrace PRIVATE /home/cluangrath/pti-gpu/tools/unitrace/build/ittheaders/)  # replace this
target_link_libraries(colintrace PRIVATE pthread dl)
