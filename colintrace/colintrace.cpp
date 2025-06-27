#include "colintrace.h"
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

// A thread-safe, buffered writer for our trace file
class ThreadBuffer {
public:
    ThreadBuffer(FILE* file) : trace_file(file), buffer_needs_comma(false) {}

    ~ThreadBuffer() {
        flush();
    }

    void write(const std::string& data) {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        if (buffer_needs_comma.exchange(true)) {
            buffer.append(",\n");
        }
        buffer.append(data);

        if (buffer.length() > 8192) {
            flush_unsafe();
        }
    }

    void flush() {
        std::lock_guard<std::mutex> lock(buffer_mutex);
        flush_unsafe();
    }

private:
    void flush_unsafe() {
        if (!buffer.empty() && trace_file) {
            fwrite(buffer.c_str(), 1, buffer.length(), trace_file);
            buffer.clear();
        }
    }

    FILE* trace_file;
    std::string buffer;
    std::mutex buffer_mutex;
    std::atomic<bool> buffer_needs_comma;
};

// PIMPL idiom to hide implementation details
class IttCollectorImpl {
public:
    IttCollectorImpl() : trace_file(nullptr) {
        char fname[128];
        snprintf(fname, sizeof(fname), "trace.pid_%d.json", getpid());
        trace_file = fopen(fname, "w");
        if (trace_file) {
            fprintf(trace_file, "{\"traceEvents\": [");
            std::cerr << "[colintrace] Loaded for PID " << getpid() << ". Logging to " << fname << std::endl;
        } else {
            std::cerr << "[colintrace] ERROR: Failed to open log file '" << fname << "'. Reason: " << strerror(errno) << std::endl;
        }
    }

    ~IttCollectorImpl() {
        if (trace_file) {
            // Write any remaining buffered data
            for (auto const& [key, val] : thread_buffers) {
                val->flush();
            }
            fprintf(trace_file, "\n]}\n");
            fclose(trace_file);
            std::cerr << "[colintrace] Finalized trace for PID " << getpid() << std::endl;
        }
    }

    void Log(const std::string& name, uint64_t start, uint64_t end) {
        if (!trace_file) return;

        std::stringstream ss;
        ss << "{\"name\": \"" << name << "\", \"cat\": \"task\", \"ph\": \"X\", \"ts\": " << start
           << ", \"dur\": " << (end - start) << ", \"pid\": " << getpid()
           << ", \"tid\": " << get_my_tid() << ", \"args\": {}}";

        get_thread_buffer()->write(ss.str());
    }

private:
    static int get_my_tid() {
        #ifdef __linux__
            return syscall(SYS_gettid);
        #else
            static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
            return an_id;
        #endif
    }

    ThreadBuffer* get_thread_buffer() {
        std::thread::id this_id = std::this_thread::get_id();
        std::lock_guard<std::mutex> lock(thread_buffers_mutex);
        if (thread_buffers.find(this_id) == thread_buffers.end()) {
            thread_buffers[this_id] = std::make_unique<ThreadBuffer>(trace_file);
        }
        return thread_buffers[this_id].get();
    }

    FILE* trace_file;
    std::mutex thread_buffers_mutex;
    std::map<std::thread::id, std::unique_ptr<ThreadBuffer>> thread_buffers;
};


// --- IttCollector implementation ---
IttCollector* IttCollector::GetInstance() {
    static IttCollector instance;
    return &instance;
}

IttCollector::IttCollector() {
    pimpl = new IttCollectorImpl();
}

IttCollector::~IttCollector() {
    delete pimpl;
}

void IttCollector::Log(const std::string& name, uint64_t start, uint64_t end) {
    pimpl->Log(name, start, end);
}

// --- ITT API Hooks ---
namespace {
    struct TaskInfo {
        std::string name;
        uint64_t start_time;
    };

    static thread_local std::stack<TaskInfo> g_task_stack;

    static uint64_t get_the_time() {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
}

extern "C" {
    void ITTAPI collector_task_begin(const __itt_domain* domain, __itt_id, __itt_id, __itt_string_handle* name) {
        if (!domain || !(domain->flags & 1) || !name || !name->strA) return;
        g_task_stack.push({std::string(domain->nameA) + "::" + std::string(name->strA), get_the_time()});
    }

    void ITTAPI collector_task_end(const __itt_domain* domain) {
        if (!domain || !(domain->flags & 1) || g_task_stack.empty()) return;
        TaskInfo task = g_task_stack.top();
        g_task_stack.pop();
        IttCollector::GetInstance()->Log(task.name, task.start_time, get_the_time());
    }

    __itt_domain* ITTAPI collector_domain_create(const char* name) {
        if (!name) return nullptr;
        __itt_domain* d = new __itt_domain();
        d->flags = 1;
        d->nameA = new char[strlen(name) + 1];
        strcpy(const_cast<char*>(d->nameA), name);
        d->nameW = nullptr;
        return d;
    }

    __itt_string_handle* ITTAPI collector_string_handle_create(const char* name) {
        if (!name) return nullptr;
        __itt_string_handle* h = new __itt_string_handle();
        h->strA = new char[strlen(name) + 1];
        strcpy(const_cast<char*>(h->strA), name);
        h->strW = nullptr;
        return h;
    }

    __itt_domain_create_ptr__3_0_t          __itt_domain_create_ptr__3_0 = &collector_domain_create;
    __itt_string_handle_create_ptr__3_0_t   __itt_string_handle_create_ptr__3_0 = &collector_string_handle_create;
    __itt_task_begin_ptr__3_0_t             __itt_task_begin_ptr__3_0 = &collector_task_begin;
    __itt_task_end_ptr__3_0_t               __itt_task_end_ptr__3_0 = &collector_task_end;
    int __itt_api_version_3_0 = 1;
}
