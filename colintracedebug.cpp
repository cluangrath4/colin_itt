/*
 * Name: Colin Luangrath
 * Date: June 12, 2025
 * Description: A lightweight C++ tracer using the Intel ITT API to generate
 * trace files in JSON format for Perfetto. This revised version
 * improves thread safety and file handling for HPC environments.
*/

#include <iostream>
#include <stack>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <atomic>
#include <sstream>

#include "ittnotify.h"

// Undefine the ITT macros to ensure our custom functions are called.
#undef __itt_task_begin
#undef __itt_task_end

// === Global State ===

// Thread-local stack to store the start timestamps of nested tasks.
// Each thread gets its own stack.
thread_local std::stack<uint64_t> g_task_start_stack;

// A structure to manage the trace file handle and its state.
struct TraceFile {
    FILE* handle = nullptr;
    bool is_first_entry = true; // Used to manage JSON comma separation.
};

// Global pointer to the trace file manager, protected by a mutex.
static TraceFile g_trace_file;
static std::mutex g_file_mutex;


// === Utility Functions ===

/**
 * @brief Get the current time as microseconds since epoch.
 * @return The current timestamp.
 */
uint64_t get_timestamp_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

/**
 * @brief Get a unique identifier for the current thread.
 * std::hash is sufficient for distinguishing threads within a process.
 * @return A numeric thread identifier.
 */
int get_thread_id() {
    static thread_local int thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return thread_id;
}


// === Core Logic ===

/**
 * @brief This function is called when the library is loaded.
 * It prints a message to stderr to confirm that the tracer is active.
 */
__attribute__((constructor))
void initialize_tracer() {
    // Open the trace file as soon as the library is loaded.
    // This avoids race conditions during the first task call.
    std::lock_guard<std::mutex> lock(g_file_mutex);

    if (g_trace_file.handle == nullptr) {
        char filename[128];
        snprintf(filename, sizeof(filename), "trace.%d.json", getpid());

        g_trace_file.handle = fopen(filename, "w");
        if (g_trace_file.handle) {
            fprintf(stderr, "[Tracer] Loaded for PID %d. Logging to %s\n", getpid(), filename);
            fprintf(g_trace_file.handle, "{\"traceEvents\": [\n"); // Start of JSON array
        } else {
            fprintf(stderr, "[Tracer] ERROR: PID %d failed to open log file '%s'. Reason: %s\n", getpid(), filename, strerror(errno));
        }
    }
}

/**
 * @brief This function is called when the library is unloaded or the program exits.
 * It ensures the JSON file is correctly finalized.
 */
__attribute__((destructor))
void finalize_tracer() {
    std::lock_guard<std::mutex> lock(g_file_mutex);
    
    if (g_trace_file.handle) {
        fprintf(stderr, "[Tracer] Unloading for PID %d. Finalizing trace file.\n", getpid());
        fprintf(g_trace_file.handle, "\n{}\n]}"); // End of JSON array, {} is a dummy object to handle trailing comma.
        fclose(g_trace_file.handle);
        g_trace_file.handle = nullptr;
    } else {
        fprintf(stderr, "[Tracer] Unloading for PID %d. No trace file was opened.\n", getpid());
    }
}


// === ITT API Overrides ===

extern "C" {

/**
 * @brief Overrides the default __itt_task_begin.
 * Called by the application to mark the beginning of a task.
 * @param domain The ITT domain of the task.
 * @param taskid (unused) The ITT task ID.
 * @param parentid (unused) The ITT parent task ID.
 * @param name The string handle for the task's name.
 */
void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) {
    // We only need the start time. The rest of the data will be written in __itt_task_end.
    // This is more efficient and robust against crashes.
    uint64_t start_time = get_timestamp_us();
    g_task_start_stack.push(start_time);
}

/**
 * @brief Overrides the default __itt_task_end.
 * Called by the application to mark the end of a task.
 * @param domain The ITT domain of the task. We use it to get the task name.
 */
void __itt_task_end(const __itt_domain* domain) {
    uint64_t end_time = get_timestamp_us();

    // If the stack is empty, it's a stray 'end' call. Ignore it.
    if (g_task_start_stack.empty()) {
        return;
    }

    uint64_t start_time = g_task_start_stack.top();
    g_task_start_stack.pop();
    
    uint64_t duration = end_time - start_time;

    // The name of the task is often stored in the domain for CCL calls.
    const char* task_name = (domain && domain->nameA) ? domain->nameA : "unknown_task";

    // Lock before accessing the global file handle.
    std::lock_guard<std::mutex> lock(g_file_mutex);
    
    if (!g_trace_file.handle) {
        return; // Cannot write if the file isn't open.
    }

    // Add a comma before each entry except the very first one.
    if (g_trace_file.is_first_entry) {
        g_trace_file.is_first_entry = false;
    } else {
        fprintf(g_trace_file.handle, ",\n");
    }

    // Write a "Complete Event" (ph: "X"). This is more efficient than "B" and "E" events.
    // It combines begin and end into a single entry.
    fprintf(g_trace_file.handle,
            "{\"name\": \"%s\", \"cat\": \"CCL\", \"ph\": \"X\", \"ts\": %llu, \"dur\": %llu, \"pid\": %d, \"tid\": %d}",
            task_name,
            (unsigned long long)start_time,
            (unsigned long long)duration,
            getpid(),
            get_thread_id());
    
    // It's good practice to flush periodically to avoid data loss on crashes,
    // but flushing on every event can be slow. A compromise is to flush here.
    fflush(g_trace_file.handle);
}

} // extern "C"
