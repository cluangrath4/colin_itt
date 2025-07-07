# minibench/CMakeLists.txt
add_executable(minibench minibench.cpp)

# We still need the main include directory for the ITT headers.
target_include_directories(minibench PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link ONLY against libittnotify.so.
# The direct dependency on 'colintrace' has been removed.
target_link_libraries(minibench PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/libittnotify.so
)

# Set the runtime path (rpath) for the executable.
# It now only needs to find libittnotify.so.
set(RPATH_ITTNOTIFY "${PROJECT_SOURCE_DIR}/lib")

target_link_options(minibench PRIVATE
    "-Wl,-rpath,${RPATH_ITTNOTIFY}"
)
