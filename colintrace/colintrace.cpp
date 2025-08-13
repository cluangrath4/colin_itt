/*
 * Standalone ITTAPI tracer
 * Overrides ITTAPI functions using LD_PRELOAD
 * Outputs JSON trace
 */

#define INTEL_NO_MACRO_BODY
#include "colintrace.h"

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
#include <unistd.h>
#include <syscall.h>

// --- Global State ---
static std::ofstream* g_trace_file_ptr = nullptr;
static std::mutex g_file_mutex;
static std::atomic<bool> g_is_first_event{true};

// Maps to store created domains and string handles, ensuring pointer identity
static std::mutex g_domain_mutex;
static std::map<std::string, __itt_domain*> g_domain_map;

static std::mutex g_string_handle_mutex;
static std::map<std::string, __itt_string_handle*> g_string_handle_map;

// Global state for ITT events
static std::mutex g_event_mutex;
static std::vector<std::string> g_event_names;

struct TaskInfo {
    std::string name;
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;
};

// Per-thread storage for ongoing events and tasks
static thread_local std::stack<TaskInfo> g_task_stack;
static thread_local std::map<__itt_event, std::chrono::time_point<std::chrono::high_resolution_clock>> g_event_start_times;


// --- Utility Functions ---
static long long get_time_us() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

static void write_trace_entry(const std::string& entry) {
    std::lock_guard<std::mutex> lock(g_file_mutex);
    if (g_trace_file_ptr && g_trace_file_ptr->is_open()) {
        if (!g_is_first_event.exchange(false)) {
            (*g_trace_file_ptr) << ",\n";
        }
        (*g_trace_file_ptr) << entry;
    }
}

// --- Constructor / Destructor ---
__attribute__((constructor))
void tracer_init() {
    g_trace_file_ptr = new std::ofstream();
    std::string filename = "trace.pid_" + std::to_string(getpid()) + ".json";
    g_trace_file_ptr->open(filename);
    (*g_trace_file_ptr) << "{\"traceEvents\": [\n";
    std::cerr << "[colintrace] Tracer loaded. Logging to " << filename << std::endl;
}

__attribute__((destructor))
void tracer_cleanup() {
    if (g_trace_file_ptr && g_trace_file_ptr->is_open()) {
        (*g_trace_file_ptr) << "\n]}\n";
        g_trace_file_ptr->close();
        delete g_trace_file_ptr;
        g_trace_file_ptr = nullptr;
        std::cerr << "[colintrace] Tracer finalized." << std::endl;
    }
}

// --- Overridden ITT API Functions ---
extern "C" {

// --- Domain and String Handle Management (unitrace style) ---
__itt_domain* __itt_domain_create(const char* name) {
    if (!name) return nullptr;
    std::lock_guard<std::mutex> lock(g_domain_mutex);
    auto it = g_domain_map.find(name);
    if (it != g_domain_map.end()) {
        return it->second; // Return existing domain
    }
    // Create new domain if it doesn't exist
    __itt_domain* d = new __itt_domain();
    d->flags = 1; // Mark as enabled
    char* name_copy = new char[strlen(name) + 1];
    strcpy(name_copy, name);
    d->nameA = name_copy;
    d->nameW = nullptr;
    g_domain_map[name] = d;
    return d;
}

__itt_string_handle* __itt_string_handle_create(const char* name) {
    if (!name) return nullptr;
    std::lock_guard<std::mutex> lock(g_string_handle_mutex);
    auto it = g_string_handle_map.find(name);
    if (it != g_string_handle_map.end()) {
        return it->second; // Return existing handle
    }
    // Create new handle if it doesn't exist
    __itt_string_handle* h = new __itt_string_handle();
    char* name_copy = new char[strlen(name) + 1];
    strcpy(name_copy, name);
    h->strA = name_copy;
    h->strW = nullptr;
    g_string_handle_map[name] = h;
    return h;
}

// --- Task Tracing ---
void __itt_task_begin(const __itt_domain* domain, __itt_id, __itt_id, __itt_string_handle* name) {
    if (!domain || !(domain->flags & 1) || !name || !name->strA) return;
    g_task_stack.push({std::string(domain->nameA) + "::" + std::string(name->strA),
                       std::chrono::high_resolution_clock::now()});
}

void __itt_task_end(const __itt_domain* domain) {
    if (!domain || !(domain->flags & 1) || g_task_stack.empty()) return;

    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();

    long long start_us = std::chrono::duration_cast<std::chrono::microseconds>(task.start_time.time_since_epoch()).count();
    long long end_us = get_time_us();
    long long duration_us = end_us - start_us;

    std::string entry = "{\"name\": \"" + task.name + "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " + std::to_string(start_us) +
             ", \"dur\": " + std::to_string(duration_us) + ", \"pid\": " + std::to_string(getpid()) +
             ", \"tid\": " + std::to_string(syscall(SYS_gettid)) + "}";

    write_trace_entry(entry);
}

// --- Event Tracing ---
__itt_event __itt_event_create(const char* name, int namelen) {
    std::lock_guard<std::mutex> lock(g_event_mutex);
    g_event_names.emplace_back(name, namelen);
    return g_event_names.size() - 1;
}

int __itt_event_start(__itt_event event) {
    if (event >= g_event_names.size()) return -1;
    g_event_start_times[event] = std::chrono::high_resolution_clock::now();
    return 0;
}

int __itt_event_end(__itt_event event) {
    if (event >= g_event_names.size() || g_event_start_times.find(event) == g_event_start_times.end()) return -1;
    auto start_time = g_event_start_times[event];
    g_event_start_times.erase(event);
    long long start_us = std::chrono::duration_cast<std::chrono::microseconds>(start_time.time_since_epoch()).count();
    long long end_us = get_time_us();
    std::string entry = "{\"name\": \"" + g_event_names[event] + "\", \"cat\": \"event\", \"ph\": \"X\", \"ts\": " + std::to_string(start_us) +
                        ", \"dur\": " + std::to_string(end_us - start_us) + ", \"pid\": " + std::to_string(getpid()) +
                        ", \"tid\": " + std::to_string(syscall(SYS_gettid)) + "}";
    write_trace_entry(entry);
    return 0;
}

// --- Marker Tracing ---
void __itt_marker(const __itt_domain* domain, __itt_id id, __itt_string_handle* name, __itt_scope scope) {
    if (!domain || !(domain->flags & 1) || !name || !name->strA) return;
    long long ts_us = get_time_us();
    std::string marker_name = std::string(domain->nameA) + "::" + std::string(name->strA);
    std::string entry = "{\"name\": \"" + marker_name + "\", \"cat\": \"marker\", \"ph\": \"R\", \"ts\": " + std::to_string(ts_us) +
                        ", \"pid\": " + std::to_string(getpid()) +
                        ", \"tid\": " + std::to_string(syscall(SYS_gettid)) + "}";
    write_trace_entry(entry);
}

// --- Empty stubs for other ITT functions to ensure binary compatibility ---
void __itt_metadata_add(const __itt_domain* domain, __itt_id id, __itt_string_handle* key, __itt_metadata_type type, size_t count, void* data) {}
void __itt_relation_add_to_current(const __itt_domain* domain, __itt_relation relation, __itt_id tail) {}
void __itt_relation_add(const __itt_domain* domain, __itt_id head, __itt_relation relation, __itt_id tail) {}
void __itt_task_group(const __itt_domain* domain, __itt_id id, __itt_id parentid, __itt_string_handle* name) {}
// Might need to add more later

} // extern "C"
