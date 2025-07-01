#include "colintrace.h"

// Standard library headers for C++
#include <iostream>
#include <fstream>
#include <stack>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>
#include <vector>
#include <map>
#include <memory>
#include <cstring>

// Headers for system
#include <unistd.h>
#ifdef __linux__
#include <syscall.h>
#endif

// Global state for tracer
// Use a pointer for the file stream to control its creation time
static std::ofstream* g_trace_file_ptr = nullptr;
static std::mutex g_file_mutex;
static std::atomic<bool> g_is_first_event{true};

struct TaskInfo {
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};

// Each thread gets its own stack of tasks
static thread_local std::stack<TaskInfo> g_task_stack;

// Helper to get time in microseconds
static long long get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

// Constructor and destructor functions for the tracer
__attribute__((constructor))
void tracer_init() {
    // Create the file stream object on the heap.
    g_trace_file_ptr = new std::ofstream();
    std::string filename = "trace.pid_" + std::to_string(getpid()) + ".json";
    g_trace_file_ptr->open(filename);
    (*g_trace_file_ptr) << "{\"traceEvents\": [\n";
    std::cerr << "[colintrace] Tracer loaded. Logging to " << filename << std::endl;
}

// This function runs automatically when the program exits
__attribute__((destructor))
void tracer_cleanup() {
    if (g_trace_file_ptr && g_trace_file_ptr->is_open()) {
        (*g_trace_file_ptr) << "\n]}\n";
        g_trace_file_ptr->close();
        // Clean up the memory for the file stream object
        delete g_trace_file_ptr;
        g_trace_file_ptr = nullptr;
        std::cerr << "[colintrace] Tracer finalized." << std::endl;
    }
}


// ITTAPI hook functions 

// This is where we define what happens when an ITT function is called
extern "C" {
    void ITTAPI collector_task_begin(const __itt_domain* domain, __itt_id, __itt_id, __itt_string_handle* name) {
        if (!domain || !(domain->flags & 1) || !name || !name->strA) return;
        g_task_stack.push({std::string(domain->nameA) + "::" + std::string(name->strA), std::chrono::high_resolution_clock::now()});
    }

    void ITTAPI collector_task_end(const __itt_domain* domain) {
        if (!domain || !(domain->flags & 1) || g_task_stack.empty()) return;
        
        TaskInfo task = g_task_stack.top();
        g_task_stack.pop();
        
        long long start_us = std::chrono::duration_cast<std::chrono::microseconds>(task.start_time.time_since_epoch()).count();
        long long end_us = get_time_us();
        long long duration_us = end_us - start_us;

        std::string entry;
        entry.reserve(256);

        if (!g_is_first_event.exchange(false)) {
            entry += ",\n";
        }
        entry += "{\"name\": \"" + task.name + "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " + std::to_string(start_us) +
                 ", \"dur\": " + std::to_string(duration_us) + ", \"pid\": " + std::to_string(getpid()) +
                 ", \"tid\": " + std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "}";

        // Lock the file to prevent multiple threads from writing at the same time
        std::lock_guard<std::mutex> lock(g_file_mutex);
        // Check if the pointer is valid before using it
        if (g_trace_file_ptr && g_trace_file_ptr->is_open()) {
            (*g_trace_file_ptr) << entry;
        }
    }

    __itt_domain* ITTAPI collector_domain_create(const char* name) {
        if (!name) return nullptr;
        __itt_domain* d = new __itt_domain();
        d->flags = 1;
        char* name_copy = new char[strlen(name) + 1];
        strcpy(name_copy, name);
        d->nameA = name_copy;
        d->nameW = nullptr;
        return d;
    }

    __itt_string_handle* ITTAPI collector_string_handle_create(const char* name) {
        if (!name) return nullptr;
        __itt_string_handle* h = new __itt_string_handle();
        char* name_copy = new char[strlen(name) + 1];
        strcpy(name_copy, name);
        h->strA = name_copy;
        h->strW = nullptr;
        return h;
    }

    // Add global pointers here
    __itt_domain_create_ptr__3_0_t          __itt_domain_create_ptr__3_0 = &collector_domain_create;
    __itt_string_handle_create_ptr__3_0_t   __itt_string_handle_create_ptr__3_0 = &collector_string_handle_create;
    __itt_task_begin_ptr__3_0_t             __itt_task_begin_ptr__3_0 = &collector_task_begin;
    __itt_task_end_ptr__3_0_t               __itt_task_end_ptr__3_0 = &collector_task_end;
    int __itt_api_version_3_0 = 1;
}
