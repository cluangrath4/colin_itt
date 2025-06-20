#pragma once

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

// So we can run this standalone, we need to add these
#ifndef UNITRACE_INTEGRATION
#include "ittnotify.h"
#endif

// For compatability
struct IttArgs {};
typedef void (*OnIttLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, IttArgs* metadata_args);
typedef void (*OnMpiLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, size_t src_size, int src_location, int src_tag, size_t dst_size, int dst_location, int dst_tag);
typedef void (*OnMpiInternalLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, int64_t mpi_counter, size_t src_size, size_t dst_size);

class IttCollector {
public:
    // Create a singleton instance of IttCollector, which is the main interface for logging ITT events.

    // Overloaded Create() to satisfy both environments. The callback is ignored.
    static IttCollector* Create(OnIttLoggingCallback callback) { return Create(); }
    static IttCollector* Create() {
        static IttCollector instance;
        return &instance;
    }

    // stubs so we can use it in unitrace context
    void EnableCclSummary() {}
    void EnableChromeLogging() {}
    std::string CclSummaryReport() { return ""; }
    void SetMpiCallback(OnMpiLoggingCallback callback) {}
    void SetMpiInternalCallback(OnMpiInternalLoggingCallback callback) {}

    // how to log an event
    static void Log(const std::string& name, uint64_t start, uint64_t end) {
        std::stringstream ss;
        if (g_thread_buffer.needs_comma.exchange(true)) {
            ss << ",\n";
        }
        // X is a complete event (start, end times))
        ss << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " << start
           << ", \"dur\": " << (end - start) << ", \"pid\": " << getpid()
           << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";
        g_thread_buffer.buffer.append(ss.str());
        if (g_thread_buffer.buffer.length() > 8192) {
            g_thread_buffer.flush_thread_buffer();
        }
    }

private:
    // This nested struct should be public to define static members of its type.
    struct ThreadBufferWrapper {
        std::string buffer;
        std::atomic<bool> needs_comma{false};
        std::atomic<bool> json_ended{false};
        ~ThreadBufferWrapper() { flush_thread_buffer(); }
        void flush_thread_buffer() {
            if (!buffer.empty()) {
                std::lock_guard<std::mutex> lock(g_file_mutex);
                FILE* f = g_trace_file.load(std::memory_order_acquire);
                if (f) {
                    fprintf(f, "%s", buffer.c_str());
                    fflush(f);
                }
                buffer.clear();
            }
        }
    };

    IttCollector() {
        char fname[64];
        sprintf(fname, "trace_pid_%d.json", getpid());
        FILE* f = fopen(fname, "w");
        if (f) fprintf(f, "{\"traceEvents\": [");
        g_trace_file.store(f, std::memory_order_release);
    }
    ~IttCollector() {
        g_thread_buffer.flush_thread_buffer();
        FILE* f = g_trace_file.load(std::memory_order_acquire);
        if (f) {
            if (g_thread_buffer.json_ended.exchange(true)) return;
            std::lock_guard<std::mutex> lock(g_file_mutex);
            fprintf(f, "\n]}\n");
            fclose(f);
            g_trace_file.store(nullptr, std::memory_order_release);
        }
    }

    static int get_my_tid() {
        #ifdef __linux__
            return syscall(SYS_gettid);
        #else
            static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return an_id;
        #endif
    }

    static std::atomic<FILE*> g_trace_file;
    static std::mutex g_file_mutex;
    static thread_local ThreadBufferWrapper g_thread_buffer;
};

std::atomic<FILE*> IttCollector::g_trace_file{nullptr};
std::mutex IttCollector::g_file_mutex;
thread_local IttCollector::ThreadBufferWrapper IttCollector::g_thread_buffer;

// If UNITRACE_INTEGRATION is not defined, we are in standalone mode.
#ifndef UNITRACE_INTEGRATION
#if __cplusplus

// forward declarations of ITT API structs
struct TaskInfo { std::string name; uint64_t start_time; };
thread_local std::stack<TaskInfo> g_task_stack;
static IttCollector* itt_collector = nullptr;
static void IttCollectorInit() { if (itt_collector == nullptr) itt_collector = IttCollector::Create(); }
static uint64_t get_the_time() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); }

// ITTAPI function implementations
static __itt_domain* ITTAPI collector_domain_create(const char* name) {
    auto* d = new __itt_domain();
    d->flags = 1; d->nameA = name; d->nameW = nullptr; d->extra1 = 0; d->extra2 = nullptr; d->next = nullptr;
    return d;
}
static __itt_string_handle* ITTAPI collector_string_handle_create(const char* name) {
    auto* h = new __itt_string_handle();
    h->strA = name; h->strW = nullptr; h->extra1 = 0; h->extra2 = nullptr; h->next = nullptr;
    return h;
}
static void ITTAPI collector_task_begin(const __itt_domain* domain, __itt_id, __itt_id, __itt_string_handle* name) {
    IttCollectorInit();
    if (!domain || !(domain->flags) || !name || !name->strA) return;
    g_task_stack.push({std::string(domain->nameA) + "::" + std::string(name->strA), get_the_time()});
}
static void ITTAPI collector_task_end(const __itt_domain* domain) {
    if (!domain || !(domain->flags) || g_task_stack.empty()) return;
    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();
    IttCollector::Log(task.name, task.start_time, get_the_time());
}

// global pointers 
extern "C" {
    __itt_domain_create_ptr__3_0_t          __itt_domain_create_ptr__3_0 = &collector_domain_create;
    __itt_string_handle_create_ptr__3_0_t   __itt_string_handle_create_ptr__3_0 = &collector_string_handle_create;
    __itt_task_begin_ptr__3_0_t             __itt_task_begin_ptr__3_0 = &collector_task_begin;
    __itt_task_end_ptr__3_0_t               __itt_task_end_ptr__3_0 = &collector_task_end;
    int __itt_api_version_3_0 = 1;
}

#endif // __cplusplus
#endif // UNITRACE_INTEGRATION