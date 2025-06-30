#include <iostream>
#include <unistd.h>
#include <vector>
#include "ittnotify.h"

// This code simulates a simple ITT test and only that.
int main() {
    std::cout << "Starting simple ITT test..." << std::endl;
    std::cout << "Process ID: " << getpid() << std::endl;
    
    __itt_domain* domain = __itt_domain_createA("test.domain");
    __itt_string_handle* task1 = __itt_string_handle_createA("main_task");
    __itt_string_handle* task2 = __itt_string_handle_createA("subtask");
    
    std::cout << "Beginning main task..." << std::endl;
    __itt_task_begin(domain, __itt_null, __itt_null, task1);
    
    std::cout << "Doing some work..." << std::endl;
    usleep(50000); // 50ms
    
    std::cout << "Beginning subtask..." << std::endl;
    __itt_task_begin(domain, __itt_null, __itt_null, task2);
    usleep(100000); // 100ms
    std::cout << "Ending subtask..." << std::endl;
    __itt_task_end(domain);
    
    usleep(200000); // 200ms
    
    std::cout << "Ending main task..." << std::endl;
    __itt_task_end(domain);
    
    std::cout << "Test complete. Check for trace.pid_" << getpid() << ".json" << std::endl;

    return 0;
}
