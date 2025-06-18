/*
 * Name: Colin Luangrath
 * Date: June 7, 2025
 * Description: This is a C++ tracer that uses the Intel ITT API to trace tasks.
 * Should create a json file with the trace data, and we can use it with perfetto
*/

#include <iostream>
#include <fstream>
#include <stack>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <atomic>
#include "ittnotify.h"

// Undefine the original macros to ensure our functions are called
#undef __itt_task_begin
#undef __itt_task_end

// global stuff
static std::mutex g_file_mutex; // lock for the file
static std::atomic<FILE*> g_trace_file{nullptr};

// so we know the name of the task when it ends
struct TaskInfo {
    std::string name;
    uint64_t start_time;
};

// get time
uint64_t get_the_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

// Gets a cached thread ID
int get_my_tid() {
    static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return an_id;
}


// a wrapper for our buffer. its destructor will be called on thread exit.
struct ThreadBufferWrapper {
    std::stringstream buffer;
    bool needs_comma = false;

    ~ThreadBufferWrapper() {
        flush_thread_buffer();
    }

    void flush_thread_buffer() {
        std::string buffer_content = buffer.str();
        if (!buffer_content.empty()) {
            std::lock_guard<std::mutex> lock(g_file_mutex);
            FILE* f = g_trace_file.load(std::memory_order_acquire);
            if (f) {
                fprintf(f, "%s", buffer_content.c_str());
                fflush(f);
            }
            buffer.str(""); // Clear the buffer after flushing
            buffer.clear();
        }
    }
};

// stuff for each thread
thread_local std::stack<TaskInfo> g_task_stack; // task stack
thread_local ThreadBufferWrapper g_thread_buffer; // each thread gets one of these wrappers


// this runs when lib is loaded
__attribute__((constructor))
void init() {
    char fname[64];
    sprintf(fname, "trace_%d.json", getpid());
    FILE* f = fopen(fname, "w");
    if (f) {
        fprintf(f, "{\"traceEvents\": [\n"); // json start
        g_trace_file.store(f, std::memory_order_release);
    }
}

__attribute__((destructor))
void cleanup() {
    g_thread_buffer.flush_thread_buffer();
    FILE* f = g_trace_file.load(std::memory_order_acquire);
    if (f) {
        std::lock_guard<std::mutex> lock(g_file_mutex);
        fprintf(f, "{}]}\n");
        fclose(f);
        g_trace_file.store(nullptr, std::memory_order_release);
    }
}

extern "C" {

// === THIS IS THE FIX ===
// Correctly allocate memory for the structs instead of the dangerous cast.
// Note: This creates a small memory leak, which is acceptable for a tracer
// that lives for the duration of the process.

__itt_domain* __itt_domain_createA(const char* name) {
    __itt_domain* domain = new __itt_domain;
    domain->nameA = name;
    return domain;
}

__itt_string_handle* __itt_string_handle_createA(const char* name) {
    __itt_string_handle* handle = new __itt_string_handle;
    handle->strA = name;
    return handle;
}

void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) {
    if (!domain || !name || !domain->nameA || !name->strA) {
        return;
    }

    uint64_t ts = get_the_time();
    std::string full_name = std::string(domain->nameA) + "::" + std::string(name->strA);

    g_task_stack.push({full_name, ts});

    if (g_thread_buffer.needs_comma) {
        g_thread_buffer.buffer << ",\n";
    }

    g_thread_buffer.buffer << "{\"name\": \"" << full_name << "\", \"cat\": \"task\", \"ph\": \"B\", \"ts\": " << ts
                   << ", \"pid\": " << getpid() << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";

    g_thread_buffer.needs_comma = true;
}

void __itt_task_end(const __itt_domain* domain) {
    if (g_task_stack.empty()) {
        return;
    }

    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();

    uint64_t end_time = get_the_time();

    if (g_thread_buffer.needs_comma) {
        g_thread_buffer.buffer << ",\n";
    }

    g_thread_buffer.buffer << "{\"name\": \"" << task.name << "\", \"cat\": \"task\", \"ph\": \"E\", \"ts\": " << end_time
                   << ", \"pid\": " << getpid() << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";

    g_thread_buffer.needs_comma = true;
}

} // extern "C"