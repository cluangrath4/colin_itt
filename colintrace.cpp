//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
// Co-authored by Colin luangrath

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

#include "ittnotify.h" // Need this for the ITT function types and definitions


// stub to replace the one from unievent.h
struct IttArgs {};

// Use callbacks for logging to keep the API shape
typedef void (*OnIttLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, IttArgs* metadata_args);
typedef void (*OnMpiLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, size_t src_size, int src_location, int src_tag,
                                        size_t dst_size, int dst_location, int dst_tag);
typedef void (*OnMpiInternalLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, int64_t mpi_counter, size_t src_size, 
                                                size_t dst_size);

// our own simple replacement for UniController::IsCollectionEnabled()
// it's on by default
static std::atomic<bool> g_collection_enabled{true};
static bool IsCollectionEnabled() {
    return g_collection_enabled.load(std::memory_order_acquire);
}


/*
    * Used to collect ITT events and write them to a JSON file for tracing
    * Uses a thread-local buffer to accumulate events and flushes them to the file
    * when collector is destroyed or when the buffer is full
    */
class IttCollector {
public:
    // Creates a collector if it doesn't already exist
    static IttCollector* Create(OnIttLoggingCallback callback) {
        if (collector_ == nullptr) {
            collector_ = new IttCollector();
        }
        return collector_;
    }

    // Returns the instance of the collector
    ~IttCollector() {
        g_thread_buffer.flush_thread_buffer();
        FILE* f = g_trace_file.load(std::memory_order_acquire);
        if (f) {
            std::lock_guard<std::mutex> lock(g_file_mutex);
            fprintf(f, "{}]}\n");
            fclose(f);
            g_trace_file.store(nullptr, std::memory_order_release);
        }
    }

    // Stub functions to match the ITT API even though they are not used for our purposes
    void EnableCclSummary() {}
    void EnableChromeLogging() {}
    std::string CclSummaryReport() { return ""; }
    void SetMpiCallback(OnMpiLoggingCallback callback) {}
    void SetMpiInternalCallback(OnMpiInternalLoggingCallback callback) {}


    // Our actual logging logic
    static void Log(const std::string& name, uint64_t start, uint64_t end) {
        if (g_thread_buffer.needs_comma) {
            g_thread_buffer.buffer << ",\n";
        }

        g_thread_buffer.buffer << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"B\", \"ts\": " << start
                       << ", \"pid\": " << getpid() << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";

        g_thread_buffer.buffer << ",\n";

        g_thread_buffer.buffer << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"E\", \"ts\": " << end
                       << ", \"pid\": " << getpid() << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";

        g_thread_buffer.needs_comma = true;
    }

private:
    // Prints the initial JSON header to the trace file
    // This is called when the collector is created
    IttCollector() {
        char fname[64];
        sprintf(fname, "trace_pid_%d.json", getpid());
        FILE* f = fopen(fname, "w");
        if (f) {
            fprintf(f, "{\"traceEvents\": [\n");
            g_trace_file.store(f, std::memory_order_release);
        }
    }

    static int get_my_tid() {
        static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
        return an_id;
    }

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
                buffer.str("");
                buffer.clear();
            }
        }
    };

    static IttCollector* collector_;
    static std::atomic<FILE*> g_trace_file;
    static std::mutex g_file_mutex;
    static thread_local ThreadBufferWrapper g_thread_buffer;
};

// Initialize the static members
IttCollector* IttCollector::collector_ = nullptr;
std::atomic<FILE*> IttCollector::g_trace_file{nullptr};
std::mutex IttCollector::g_file_mutex;
thread_local IttCollector::ThreadBufferWrapper IttCollector::g_thread_buffer;

// This global instance is used by the ITT functions
static IttCollector* itt_collector = nullptr;


// --- ITT API Implementation ---
#undef __itt_task_begin
#undef __itt_task_end

extern "C" {

struct TaskInfo {
    std::string name;
    uint64_t start_time;
};

thread_local std::stack<TaskInfo> g_task_stack;

static uint64_t get_the_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now.time_since_epoch()).count();
}

// this function is called by the application to create the collector
void IttCollectorInit() {
    if (itt_collector == nullptr) {
        itt_collector = IttCollector::Create(nullptr);
    }
}

void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) {
    if (!IsCollectionEnabled()) return;
    IttCollectorInit(); // Ensure collector is created
    if (!domain || !name || !domain->nameA || !name->strA) return;
    
    uint64_t ts = get_the_time();
    std::string full_name = std::string(domain->nameA) + "::" + std::string(name->strA);
    g_task_stack.push({full_name, ts});
}

void __itt_task_end(const __itt_domain* domain) {
    if (!IsCollectionEnabled()) return;
    if (g_task_stack.empty()) return;

    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();
    uint64_t end_time = get_the_time();

    IttCollector::Log(task.name, task.start_time, end_time);
}

} // extern "C"