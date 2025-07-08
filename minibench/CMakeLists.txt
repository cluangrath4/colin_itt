add_executable(minibench minibench.cpp)

# We still need the main include directory for the ITT headers
target_include_directories(minibench PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link against libittnotify.so
target_link_libraries(minibench PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/libittnotify.so
)

# Set the runtime path (rpath) for the executable
set(RPATH_ITTNOTIFY "${PROJECT_SOURCE_DIR}/lib")

# Make sure the executable can find the ITTAPI library at runtime
target_link_options(minibench PRIVATE
    "-Wl,-rpath,${RPATH_ITTNOTIFY}"
)
