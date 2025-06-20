// minibench.cpp - REVISED
#include <iostream>
#include <unistd.h>
#include <vector>

// Include the shared header file that declares the ITT API.
#include "ittnotify.h"

int main() {
    std::cout << "Starting simple ITT test..." << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;
    
    // Create domain and task handles. These calls will be linked to your tracer's implementation.
    __itt_domain* domain = __itt_domain_create_A("test.domain");
    __itt_string_handle* task1 = __itt_string_handle_create_A("main_task");
    __itt_string_handle* task2 = __itt_string_handle_create_A("subtask");
    
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
    std::cout << "Ending subtask..." << std::endl;
    __itt_task_end(domain);
    
    // More work
    usleep(200000); // 200ms
    
    std::cout << "Ending main task..." << std::endl;
    __itt_task_end(domain);
    
    std::cout << "Test complete. Check for trace_pid_" << getpid() << ".json" << std::endl;
    
    // Note: The handles are allocated in the tracer. In a real-world scenario,
    // you would also need a way to free this memory.
    delete domain;
    delete task1;
    delete task2;

    return 0;
}