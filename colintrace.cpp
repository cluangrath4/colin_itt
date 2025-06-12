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

#undef __itt_task_begin
#undef __itt_task_end


// global stuff
thread_local std::stack<uint64_t> g_task_start_stack; // task stack
std::mutex m; // lock
std::atomic<FILE*> trace_file_ptr{nullptr};

bool first = true; // for the comma 


// get time
uint64_t get_the_time() {
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(
        now.time_since_epoch()
    ).count();
}

int get_my_tid() {
    static thread_local int an_id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return an_id;
}


// this gets the file
FILE* getTheTraceFile() {
    if (trace_file_ptr.load(std::memory_order_acquire) == nullptr) {
        
        m.lock();
        // check again just in case
        if (trace_file_ptr.load(std::memory_order_relaxed) == nullptr) {
            char fname[50];
            sprintf(fname, "trace_%d.json", getpid());
            
            fprintf(stderr, "DEBUG: trying to open file %s\n", fname);
            
            FILE* f = fopen(fname, "w");
            if (f) {
                fprintf(stderr, "DEBUG: opened file %s\n", fname);
                fprintf(f, "[\n"); // json start
                trace_file_ptr.store(f, std::memory_order_release);
            } else {
                fprintf(stderr, "!!! ERROR !!! pid %d failed to open log file '%s'. Error: %s\n", getpid(), fname, strerror(errno));
            }
        }
        m.unlock();
    }
    return trace_file_ptr.load(std::memory_order_acquire);
}

// this runs when lib is loaded
__attribute__((constructor))
void init() {
    printf("[Tracer] Loaded for pid %d\n", getpid());
}

__attribute__((destructor))
void cleanup() {
    fprintf(stderr, "[Tracer] Unloading for pid %d\n", getpid());
    FILE* f = trace_file_ptr.load(std::memory_order_acquire);
    if (f) {
        // lock to be safe
        m.lock();
        fprintf(stderr, "Closing file...\n");
        fprintf(f, "\n]\n");
        fclose(f);
        trace_file_ptr.store(nullptr, std::memory_order_release);
        m.unlock();
    } else {
        // this is probably fine (fine as in ok to print but a problem lol)
        fprintf(stderr, "[Tracer] No file to close.\n");
    }
}


extern "C" {

void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) 
{
    fprintf(stderr, "-> task begin\n");
    
    if (!domain || !name || !domain->nameA || !name->strA) {
        return; // just exit
    }

    FILE* f = getTheTraceFile();
    if (f==NULL) {
        fprintf(stderr, "ERROR file is null\n");
        return;
    }
    
    uint64_t ts = get_the_time();
    g_task_start_stack.push(ts);
    
    std::string n = std::string(domain->nameA) + "::" + std::string(name->strA);

    m.lock();
    if (first) {
        // it's the first one
        first = false;
    } else {
        // not the first one (DUH)
        fprintf(f, ",\n");
    }

    fprintf(f, "{\"name\": \"%s\", \"cat\": \"task\", \"ph\": \"B\", \"ts\": %llu, \"pid\": %d, \"tid\": %d, \"args\": {}}", n.c_str(), (unsigned long long)ts, getpid(), get_my_tid());
    m.unlock();
    
    // fflush(f); //TODO: is this slow? maybe remove
}

void __itt_task_end(const __itt_domain* domain)
{
    fprintf(stderr, "<- task end\n");
    
    if (g_task_start_stack.empty()) {
        return;
    }
    
    FILE* f = getTheTraceFile();
    if (!f) {
        // no file, can't do anything
        return;
    }

    uint64_t end_time = get_the_time();
    g_task_start_stack.pop();

    m.lock();
    
    // just write the end event
    fprintf(f, ",\n{\"name\": \"\", \"cat\": \"task\", \"ph\": \"E\", \"ts\": %llu, \"pid\": %d, \"tid\": %d, \"args\": {}}", (unsigned long long)end_time, getpid(), get_my_tid());

    m.unlock();

    fflush(f);
}

} // extern "C"
