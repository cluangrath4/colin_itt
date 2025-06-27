#pragma once

#include "ittnotify.h"
#include <cstdint>
#include <string>

// Forward declare the collector implementation
class IttCollectorImpl;

class IttCollector {
public:
    static IttCollector* GetInstance();
    void Log(const std::string& name, uint64_t start, uint64_t end);

private:
    IttCollector();
    ~IttCollector();
    IttCollector(const IttCollector&) = delete;
    IttCollector& operator=(const IttCollector&) = delete;

    IttCollectorImpl* pimpl;
};
