#include <iostream>
#include <unistd.h>
#include <vector>

// This header declares the ITT API functions (e.g., __itt_task_begin).
// The actual implementation will be provided by an external tracer at runtime.
#include "ittnotify.h"

int main() {
    std::cout << "Starting simple ITT test..." << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;

    // Corrected function names for Linux.
    __itt_domain* domain = __itt_domain_create("test.domain");
    __itt_string_handle* task1 = __itt_string_handle_create("main_task");
    __itt_string_handle* task2 = __itt_string_handle_create("subtask");

    std::cout << "Beginning main task..." << std::endl;
    __itt_task_begin(domain, __itt_null, __itt_null, task1);

    usleep(50000); // 50ms of work

    std::cout << "Beginning subtask..." << std::endl;
    __itt_task_begin(domain, __itt_null, __itt_null, task2);
    usleep(100000); // 100ms of work
    std::cout << "Ending subtask..." << std::endl;
    __itt_task_end(domain);

    usleep(200000); // 200ms of work

    std::cout << "Ending main task..." << std::endl;
    __itt_task_end(domain);

    std::cout << "Test complete." << std::endl;

    return 0;
}

extern "C" {
    void* __itt_domain_create_ptr__3_0 __attribute__((weak));
    void* __itt_string_handle_create_ptr__3_0 __attribute__((weak));
    void* __itt_task_begin_ptr__3_0 __attribute__((weak));
    void* __itt_task_end_ptr__3_0 __attribute__((weak));
}
