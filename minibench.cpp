// minibench.cpp - Test program for ITT tracer
#include <iostream>
#include <unistd.h>
#include <dlfcn.h>

// Forward declare the ITT functions that tracer implements
extern "C" {
    typedef struct ___itt_domain {
        const char* nameA;
    } __itt_domain;
    
    typedef struct ___itt_string_handle {
        const char* strA;
    } __itt_string_handle;
    
    typedef unsigned long long __itt_id;
    
    // Function pointer types
    typedef void (*itt_task_begin_t)(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name);
    typedef void (*itt_task_end_t)(const __itt_domain* domain);
}

// Simple implementations that create the structs
__itt_domain* __itt_domain_create(const char* domain) {
    static __itt_domain d;
    d.nameA = domain;
    return &d;
}

__itt_string_handle* __itt_string_handle_create(const char* name) {
    static __itt_string_handle h;
    h.strA = name;
    return &h;
}

// Dynamic ITT function wrappers
void __itt_task_begin(const __itt_domain* domain, __itt_id taskid, __itt_id parentid, __itt_string_handle* name) {
    // Try to find the function dynamically first
    static itt_task_begin_t func = nullptr;
    static bool looked_up = false;
    
    if (!looked_up) {
        func = (itt_task_begin_t)dlsym(RTLD_DEFAULT, "__itt_task_begin");
        looked_up = true;
        if (func) {
            std::cout << "[DYNAMIC] Found __itt_task_begin in loaded library" << std::endl;
        } else {
            std::cout << "[DYNAMIC] __itt_task_begin not found - using stub" << std::endl;
        }
    }
    
    if (func) {
        func(domain, taskid, parentid, name);
    } else {
        std::cout << "[STUB] __itt_task_begin called (no tracer loaded)" << std::endl;
    }
}

void __itt_task_end(const __itt_domain* domain) {
    // Try to find the function dynamically first
    static itt_task_end_t func = nullptr;
    static bool looked_up = false;
    
    if (!looked_up) {
        func = (itt_task_end_t)dlsym(RTLD_DEFAULT, "__itt_task_end");
        looked_up = true;
        if (func) {
            std::cout << "[DYNAMIC] Found __itt_task_end in loaded library" << std::endl;
        } else {
            std::cout << "[DYNAMIC] __itt_task_end not found - using stub" << std::endl;
        }
    }
    
    if (func) {
        func(domain);
    } else {
        std::cout << "[STUB] __itt_task_end called (no tracer loaded)" << std::endl;
    }
}

int main() {
    std::cout << "Starting simple ITT test..." << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // Create domain and task
    __itt_domain* domain = __itt_domain_create("test.domain");
    __itt_string_handle* task1 = __itt_string_handle_create("main_task");
    __itt_string_handle* task2 = __itt_string_handle_create("subtask");
    
    std::cout << "Created domain: " << domain->nameA << std::endl;
    std::cout << "Created tasks: " << task1->strA << ", " << task2->strA << std::endl;
    
    // Test nested tasks
    std::cout << "Beginning main task..." << std::endl;
    __itt_task_begin(domain, 0, 0, task1);
    
    std::cout << "Doing some work..." << std::endl;
    
    // Nested task
    std::cout << "Beginning subtask..." << std::endl;
    __itt_task_begin(domain, 1, 0, task2);
    usleep(100000); // 100ms
    std::cout << "What the sigma" << std::endl;
    std::cout << "Ending subtask..." << std::endl;
    __itt_task_end(domain);
    
    // More work
    usleep(200000); // 200ms
    
    std::cout << "Ending main task..." << std::endl;
    __itt_task_end(domain);
    
    std::cout << "Test complete. Check for trace_rank_" << getpid() << ".log" << std::endl;
    
    return 0;
}
