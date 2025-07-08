# add shared library for colintrace
# This library will provide the ITTAPI tracing functionality
add_library(colintrace SHARED colintrace.cpp)

# Include ittnotify.h for the ITTAPI functions
target_include_directories(colintrace PUBLIC
    ${PROJECT_SOURCE_DIR}/include
)
# Link with pthread
target_link_libraries(colintrace PRIVATE pthread)
