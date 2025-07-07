add_library(colintrace SHARED colintrace.cpp)

target_include_directories(colintrace PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(colintrace PRIVATE pthread)
