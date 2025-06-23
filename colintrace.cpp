/*
 * Name: colintrace
 * Description: A standalone C++ tracer using the Intel ITT API to generate
 * trace files in JSON format for Perfetto. This version is
 * designed for thread-safety and correct ITT API hooking.
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

#ifdef __linux__
#include <syscall.h>
#endif

#include "ittnotify.h"

// statements to handle global stuff and file management

static std::mutex g_file_mutex;
static std::atomic<FILE*> g_trace_file{nullptr};
static std::atomic<bool> g_json_started{false};
static std::atomic<bool> g_is_first_entry{true};

// local event buffer

struct ThreadBuffer {
    std::stringstream buffer;
    ~ThreadBuffer() {
        flush();
    }

    void flush() {
        std::string content = buffer.str();
        if (!content.empty()) {
            std::lock_guard<std::mutex> lock(g_file_mutex);
            FILE* f = g_trace_file.load(std::memory_order_acquire);
            if (f) {
                fprintf(f, "%s", content.c_str());
                fflush(f);
            }
            buffer.str("");
            buffer.clear();
        }
    }
};

static thread_local ThreadBuffer g_thread_buffer;

// util/task information (barebones is what we need)

struct TaskInfo {
    std::string name;
    uint64_t start_time;
};

static thread_local std::stack<TaskInfo> g_task_stack;

static uint64_t get_the_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

static int get_my_tid() {
#ifdef __linux__
    return syscall(SYS_gettid);
#else
    static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return an_id;
#endif
}

// logging logic

void log_event(const std::string& name, uint64_t start, uint64_t duration) {
    std::stringstream ss;
    if (!g_is_first_entry.exchange(false)) {
        ss << ",\n";
    }
    ss << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " << start
       << ", \"dur\": " << duration << ", \"pid\": " << getpid()
       << ", \"tid\": " << get_my_tid() << "}";

    g_thread_buffer.buffer << ss.str();

    if (g_thread_buffer.buffer.tellp() > 8192) {
        g_thread_buffer.flush();
    }
}


// ITTAPI function implementations

void ITTAPI collector_task_begin(const __itt_domain* domain, __itt_id, __itt_id, __itt_string_handle* name) {
    if (!domain || !(domain->flags & 1) || !name || !name->strA) return;
    g_task_stack.push({std::string(domain->nameA) + "::" + std::string(name->strA), get_the_time()});
}

void ITTAPI collector_task_end(const __itt_domain* domain) {
    if (!domain || !(domain->flags & 1) || g_task_stack.empty()) return;
    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();
    log_event(task.name, task.start_time, get_the_time());
}

__itt_domain* ITTAPI collector_domain_create(const char* name) {
    if (!name) return nullptr;
    __itt_domain* d = new __itt_domain();
    d->flags = 1;
    d->nameA = name;
    d->nameW = nullptr;
    return d;
}

__itt_string_handle* ITTAPI collector_string_handle_create(const char* name) {
    if (!name) return nullptr;
    __itt_string_handle* h = new __itt_string_handle();
    h->strA = name;
    h->strW = nullptr;
    return h;
}


// constructor and destructor

__attribute__((constructor))
void tracer_init() {
    if (g_json_started.exchange(true)) return;
    char fname[128];
    snprintf(fname, sizeof(fname), "trace.pid_%d.json", getpid());
    FILE* f = fopen(fname, "w");
    if (f) {
        g_trace_file.store(f, std::memory_order_release);
        fprintf(f, "{\"traceEvents\": [");
        std::cerr << "[colintrace] Loaded for PID " << getpid() << ". Logging to " << fname << std::endl;
    } else {
        std::cerr << "[colintrace] ERROR: Failed to open log file '" << fname << "'. Reason: " << strerror(errno) << std::endl;
    }
}

__attribute__((destructor))
void tracer_cleanup() {
    g_thread_buffer.flush();
    FILE* f = g_trace_file.load(std::memory_order_acquire);
    if (f) {
        std::lock_guard<std::mutex> lock(g_file_mutex);
        fprintf(f, "\n{}\n]}\n");
        fclose(f);
        g_trace_file.store(nullptr, std::memory_order_release);
        std::cerr << "[colintrace] Finalized trace for PID " << getpid() << std::endl;
    }
}


// necessary ITTAPI hooks

extern "C" {
    __itt_domain_create_ptr__3_0_t          __itt_domain_create_ptr__3_0 = &collector_domain_create;
    __itt_string_handle_create_ptr__3_0_t   __itt_string_handle_create_ptr__3_0 = &collector_string_handle_create;
    __itt_task_begin_ptr__3_0_t             __itt_task_begin_ptr__3_0 = &collector_task_begin;
    __itt_task_end_ptr__3_0_t               __itt_task_end_ptr__3_0 = &collector_task_end;
    int __itt_api_version_3_0 = 1;
}
