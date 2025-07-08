/*
 * Simple function that runs ITTAPI tasks to test functionality of tracer
 * uses ITTAPI to define main and subtask with sleep calls to simulate work
 * defines INTEL_NO_MACRO_BODY *before* ittnotify allowing us to hook into colintrace if we want to (to intercept ITTAPI calls)
 * Prints to console for debugging purposes
*/
#include <iostream>
#include <unistd.h>
#include <vector>

// Define this BEFORE including ittnotify.h to use direct function calls
#define INTEL_NO_MACRO_BODY
#include "ittnotify.h"

int main() {
    std::cout << "Starting simple ITT test..." << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;

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
