// colintrace.cpp - FINAL REVISION
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

// Use the official ITT Notify header provided by the user
#include "ittnotify.h"

// need to undefine these to avoid conflicts with the ITT API
#undef __itt_domain_create_A
#undef __itt_string_handle_create_A
#undef __itt_task_begin
#undef __itt_task_end

// Forward declarations of ITT API structs
class IttCollector {
public:
    static IttCollector* Create() {
        static IttCollector instance;
        return &instance;
    }
    ~IttCollector() {
        g_thread_buffer.flush_thread_buffer();
        FILE* f = g_trace_file.load(std::memory_order_acquire);
        if (f) {
            if (g_thread_buffer.json_ended.exchange(true)) return; // Only end JSON once
            std::lock_guard<std::mutex> lock(g_file_mutex);
            fprintf(f, "\n]}\n");
            fclose(f);
            g_trace_file.store(nullptr, std::memory_order_release);
        }
    }
    static void Log(const std::string& name, uint64_t start, uint64_t end) {
        std::stringstream ss;
        if (g_thread_buffer.needs_comma.exchange(true)) {
            ss << ",\n";
        }
        ss << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " << start
           << ", \"dur\": " << (end - start) << ", \"pid\": " << getpid()
           << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";
        g_thread_buffer.buffer.append(ss.str());
        if (g_thread_buffer.buffer.length() > 8192) {
            g_thread_buffer.flush_thread_buffer();
        }
    }
private:
    IttCollector() {
        char fname[64];
        sprintf(fname, "trace_pid_%d.json", getpid());
        FILE* f = fopen(fname, "w");
        if (f) {
            fprintf(f, "{\"traceEvents\": [");
            g_trace_file.store(f, std::memory_order_release);
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
    static std::atomic<FILE*> g_trace_file;
    static std::mutex g_file_mutex;
    static thread_local ThreadBufferWrapper g_thread_buffer;
};
std::atomic<FILE*> IttCollector::g_trace_file{nullptr};
std::mutex IttCollector::g_file_mutex;
thread_local IttCollector::ThreadBufferWrapper IttCollector::g_thread_buffer;

static IttCollector* itt_collector = nullptr;


// ITTAPI implementation
extern "C" {

struct TaskInfo {
    std::string name;
    uint64_t start_time;
};
thread_local std::stack<TaskInfo> g_task_stack;

static uint64_t get_the_time() {
    return std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void IttCollectorInit() {
    static std::mutex init_mutex;
    if (itt_collector == nullptr) {
        std::lock_guard<std::mutex> lock(init_mutex);
        if (itt_collector == nullptr) {
            itt_collector = IttCollector::Create();
        }
    }
}


// lets us create domains and string handles with ITT
__itt_domain* __itt_domain_create_A(const char* name) {
    __itt_domain* d = new __itt_domain();
    d->flags = 1; // 1 means enabled
    d->nameA = name;
    d->nameW = nullptr;
    d->extra1 = 0;
    d->extra2 = nullptr;
    d->next = nullptr;
    return d;
}

__itt_string_handle* __itt_string_handle_create_A(const char* name) {
    __itt_string_handle* h = new __itt_string_handle();
    h->strA = name;
    h->strW = nullptr;
    h->extra1 = 0;
    h->extra2 = nullptr;
    h->next = nullptr;
    return h;
}

void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) {
    IttCollectorInit();
    if (!domain || !(domain->flags) || !name || !name->strA) return;

    uint64_t ts = get_the_time();
    std::string full_name = std::string(domain->nameA) + "::" + std::string(name->strA);
    g_task_stack.push({full_name, ts});
}

void __itt_task_end(const __itt_domain* domain) {
    if (!domain || !(domain->flags) || g_task_stack.empty()) return;

    TaskInfo task = g_task_stack.top();
    g_task_stack.pop();
    uint64_t end_time = get_the_time();
    IttCollector::Log(task.name, task.start_time, end_time);
}

} // extern "C"