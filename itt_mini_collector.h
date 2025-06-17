//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================

#ifndef PTI_TOOLS_UNITRACE_ITT_COLLECTOR_H_
#define PTI_TOOLS_UNITRACE_ITT_COLLECTOR_H_

#include <chrono>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <stack>
// #include <tuple> // Not needed for the minimal version
#include <map> // Required for ittFunctionInfoMap
#include <set> // Required for CclSummaryReport
#include <algorithm> // Required for std::max in CclSummaryReport


#include "unicontrol.h"
#include "unievent.h"

// --- UNNECESSARY: This is only for display purposes in the summary report. Not core to tracing.
// static std::string rank_mpi = (utils::GetEnv("PMI_RANK").empty()) ? utils::GetEnv("PMIX_RANK") : utils::GetEnv("PMI_RANK");

// --- UNNECESSARY: All callbacks are related to logging mechanisms beyond simple aggregation.
// The primary mechanism retained is AddFunctionTime, which doesn't use a callback.
/*
typedef void (*OnIttLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, IttArgs* metadata_args);
typedef void (*OnMpiLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, size_t src_size, int src_location, int src_tag,
                                      size_t dst_size, int dst_location, int dst_tag);
typedef void (*OnMpiInternalLoggingCallback)(const char *name, uint64_t start_ts, uint64_t end_ts, int64_t mpi_counter, size_t src_size, size_t dst_size);
*/

// --- NECESSARY: This struct holds the aggregated timing data for each function.
struct ittFunction {
  uint64_t total_time;
  uint64_t min_time;
  uint64_t max_time;
  uint64_t call_count;

  bool operator>(const ittFunction& r) const {
    if (total_time != r.total_time) {
      return total_time > r.total_time;
    }
    return call_count > r.call_count;
  }
  
  // --- UNNECESSARY: This operator is not used by the remaining code.
  /*
  bool operator!=(const ittFunction& r) const {
    if (total_time == r.total_time) {
      return call_count != r.call_count;
    }
    return true;
  }
  */
};
// --- NECESSARY: Typedef for the map that stores all timing data.
using ittFunctionInfoMap = std::map<std::string, ittFunction>;

// --- NECESSARY: Global map to store timing results and a mutex to protect it.
ittFunctionInfoMap ccl_function_info_map;
std::mutex lock_func_info;


// --- UNNECESSARY: This array is only for handling metadata, which is not part of core time tracing.
/*
const size_t metadata_type_sizes[] = {
    1,                  // __itt_metadata_unknown
    sizeof(uint64_t),   // __itt_metadata_u64
    sizeof(int64_t),    // __itt_metadata_s64
    sizeof(uint32_t),   // __itt_metadata_u32
    sizeof(int32_t),    // __itt_metadata_s32
    sizeof(uint16_t),   // __itt_metadata_u16
    sizeof(int16_t),    // __itt_metadata_s16
    sizeof(float),      // __itt_metadata_float
    sizeof(double)      // __itt_metadata_double
};
*/


// --- NECESSARY: This function is the core of data aggregation. It's called when a task ends.
void AddFunctionTime(const std::string& name, uint64_t time) {
  if (name.rfind("oneCCL::", 0) == 0) {
    const std::lock_guard<std::mutex> lock(lock_func_info);
    if (ccl_function_info_map.count(name) == 0) {
      ccl_function_info_map[name] = {time, time, time, 1};
    } else {
      ittFunction& function = ccl_function_info_map[name];
      function.total_time += time;
      if (time < function.min_time) {
        function.min_time = time;
      }
      if (time > function.max_time) {
        function.max_time = time;
      }
      ++function.call_count;
    }
  }
}

class IttCollector {
 public: // Interface

  // --- NECESSARY: To create the collector instance.
  static IttCollector *Create() { // Removed callback argument
    IttCollector* collector = new IttCollector(); // Removed callback argument

    if (collector == nullptr) {
      std::cerr << "[WARNING] Unable to create ITT tracer" << std::endl;
    }

    return collector;
  }

  // --- NECESSARY: To enable the summary/aggregation feature.
  void EnableCclSummary() {is_itt_ccl_summary_ = true;}
  bool IsCclSummaryOn() { return is_itt_ccl_summary_; }
  
  // --- UNNECESSARY: Chrome logging is a separate feature from simple time aggregation.
  /*
  void EnableChromeLogging() { is_itt_chrome_logging_on_ = true;}
  bool IsEnableChromeLoggingOn() { return is_itt_chrome_logging_on_; }
  */

  ~IttCollector() {
  }

  IttCollector(const IttCollector& copy) = delete;
  IttCollector& operator=(const IttCollector& copy) = delete;
  
  // --- UNNECESSARY: All Log and SetCallback methods are for features beyond simple time aggregation.
  /*
  void Log(const char *name, uint64_t start_ts, uint64_t end_ts, IttArgs* metadata_args) {
    if (callback_) {
      callback_(name, start_ts, end_ts, metadata_args);
    }
  }

  void Log(const char *name, uint64_t start_ts, uint64_t end_ts, size_t src_size, int src_location, int src_tag,
                                       size_t dst_size, int dst_location, int dst_tag) {
    if (mpi_callback_) {
      mpi_callback_(name, start_ts, end_ts, src_size, src_location, src_tag,
                                     dst_size, dst_location, dst_tag);
    }
  }

  void Log(const char *name, uint64_t start_ts, uint64_t end_ts, int64_t mpi_counter, size_t src_size, size_t dst_size) {
    if (mpi_internal_callback_) {
      mpi_internal_callback_(name, start_ts, end_ts, mpi_counter, src_size, dst_size);
    }
  }
  
  void SetMpiCallback(OnMpiLoggingCallback callback) {
    mpi_callback_ = callback;
  }

  void SetMpiInternalCallback(OnMpiInternalLoggingCallback callback) {
    mpi_internal_callback_ = callback;
  }
  */

  // --- NECESSARY: While not part of tracing, this function makes the traced data useful by reporting it.
  std::string CclSummaryReport() const {
    const uint32_t kFunctionLength = 10;
    const uint32_t kCallsLength = 12;
    const uint32_t kTimeLength = 20;
    const uint32_t kPercentLength = 12;

    if (ccl_function_info_map.empty()) {
      return "";
    }
    
    // This requires a comparator utility, assuming it's defined elsewhere.
    // Let's define a simple one here for completeness.
    struct Comparator {
        template<typename T, typename U>
        bool operator()(const std::pair<T, U>& l, const std::pair<T, U>& r) const {
            if (l.second != r.second) {
                return l.second > r.second;
            }
            return l.first > r.first;
        }
    };

    std::set< std::pair<std::string, ittFunction>,
              Comparator > sorted_list(
        ccl_function_info_map.begin(), ccl_function_info_map.end());

    uint64_t total_duration = 0;
    size_t max_name_length = kFunctionLength;
    for (auto& value : sorted_list) {
      total_duration += value.second.total_time;
      if (value.first.size() > max_name_length) {
        max_name_length = value.first.size();
      }
    }

    if (total_duration == 0) {
      return "";
    }
    std::string str;
    str += "************************************************************\n";
    // Assuming utils::GetPid() is available. Using a placeholder if not.
    // str += "* Process ID : " + std::to_string(utils::GetPid()) + " | Rank ID : " + rank_mpi + "\n";
    str += "* CCL Summary Report \n";
    str += "************************************************************\n";

    str += std::string(std::max(int(max_name_length - sizeof("Function") + 1), 0), ' ') + "Function, " +
      std::string(std::max(int(kCallsLength - sizeof("Calls") + 1), 0), ' ') + "Calls, " +
      std::string(std::max(int(kTimeLength - sizeof("Time (ns)") + 1), 0), ' ') + "Time (ns), " +
      std::string(std::max(int(kPercentLength - sizeof("Time (%)") + 1), 0), ' ') + "Time (%), " +
      std::string(std::max(int(kTimeLength - sizeof("Average (ns)") + 1), 0), ' ') + "Average (ns), " +
      std::string(std::max(int(kTimeLength - sizeof("Min (ns)") + 1), 0), ' ') + "Min (ns), " +
      std::string(std::max(int(kTimeLength - sizeof("Max (ns)") + 1), 0), ' ') + "Max (ns)\n";

    for (auto& value : sorted_list) {
      const std::string& function = value.first;
      uint64_t call_count = value.second.call_count;
      uint64_t duration = value.second.total_time;
      uint64_t avg_duration = duration / call_count;
      uint64_t min_duration = value.second.min_time;
      uint64_t max_duration = value.second.max_time;
      float percent_duration = (100.0f * duration / total_duration);
      char buffer[1024];
      snprintf(buffer, 1024, "%*s, %*llu, %*llu, %*.*f, %*llu, %*llu, %*llu\n",
        (int)max_name_length, function.c_str(),
        (int)kCallsLength, call_count,
        (int)kTimeLength, duration,
        (int)kPercentLength - 1, 2, percent_duration,
        (int)kTimeLength, avg_duration,
        (int)kTimeLength, min_duration,
        (int)kTimeLength, max_duration);
      str += buffer;
    }
    return str;
  }

 private: // Implementation

  IttCollector() { // Removed callback argument
  }

 private: // Data
  // --- UNNECESSARY: Callbacks are not used in the minimal version.
  // OnIttLoggingCallback callback_ = nullptr;
  // OnMpiLoggingCallback mpi_callback_ = nullptr;
  // OnMpiInternalLoggingCallback mpi_internal_callback_ = nullptr;
  
  // --- NECESSARY: Flag to enable the summary feature.
  bool is_itt_ccl_summary_ = false;

  // --- UNNECESSARY: Flag for a disabled feature.
  // bool is_itt_chrome_logging_on_ = false;
};

// --- NECESSARY: The global collector instance.
static IttCollector *itt_collector = nullptr;

#define INTEL_NO_MACRO_BODY
#define INTEL_ITTNOTIFY_API_PRIVATE
#include "ittnotify.h"
#include "ittnotify_config.h"

// --- NECESSARY: This struct holds the state for a currently running task.
// Metadata argument has been removed as it's an unnecessary feature.
struct ThreadTaskDescriptor {
  char domain[512];
  char name[512];
  uint64_t start_time;
  // IttArgs metadata_args; // Metadata is not necessary for start/stop timing.
};

// --- NECESSARY: The thread-local stack to manage nested task calls.
thread_local std::stack<ThreadTaskDescriptor> task_desc;


// --- UNNECESSARY: Event API is a parallel mechanism to tasks and not needed for this goal.
/*
thread_local std::map<__itt_event, uint64_t> event_desc;
static std::vector<std::string> itt_events;
static int num_itt_events = 0;
*/

// --- NECESSARY: Boilerplate for ITT stub library initialization.
static __itt_global *itt_global = NULL;
static void fill_func_ptr_per_lib(__itt_global* p)
{
  __itt_api_info* api_list = (__itt_api_info*)p->api_list_ptr;
  for (int i = 0; api_list[i].name != NULL; i++) {
    *(api_list[i].func_ptr) = (void*)__itt_get_proc(p->lib, api_list[i].name);
    if (*(api_list[i].func_ptr) == NULL)
    {
      *(api_list[i].func_ptr) = api_list[i].null_func;
    }
  }
}
ITT_EXTERN_C void ITTAPI __itt_api_init(__itt_global* p, __itt_group_id init_groups)
{
  if (p != NULL) {
    fill_func_ptr_per_lib(p);
    itt_global = p;
  }
}

// --- NECESSARY: The application calls this to create domains for tasks.
ITT_EXTERN_C __itt_domain* ITTAPI __itt_domain_create(const char *name)
{
  if (itt_global == NULL) {
    return NULL;
  }
  __itt_domain *h_tail = NULL, *h = NULL;
  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = NULL, h = itt_global->domain_list; h != NULL; h_tail = h, h = h->next) {
    if (h->nameA != NULL && !__itt_fstrcmp(h->nameA, name)) break;
  }
  if (h == NULL) {
    NEW_DOMAIN_A(itt_global, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global->mutex));
  return h;
}

// --- NECESSARY: The application calls this to create string handles for task names.
ITT_EXTERN_C __itt_string_handle* ITTAPI __itt_string_handle_create(const char* name)
{
  if (itt_global == NULL) {
    return NULL;
  }
  __itt_string_handle *h_tail = NULL, *h = NULL;
  __itt_mutex_lock(&(itt_global->mutex));
  for (h_tail = NULL, h = itt_global->string_list; h != NULL; h_tail = h, h = h->next) {
    if (h->strA != NULL && !__itt_fstrcmp(h->strA, name)) break;
  }
  if (h == NULL) {
    NEW_STRING_HANDLE_A(itt_global, h, h_tail, name);
  }
  __itt_mutex_unlock(&(itt_global->mutex));
  return h;
}


// --- UNNECESSARY: Pause/Resume control the whole collector, not individual function tracing.
/*
ITT_EXTERN_C void ITTAPI __itt_pause(void) { UniController::IttPause(); }
ITT_EXTERN_C void ITTAPI __itt_resume(void) { UniController::IttResume(); }
*/


// --- NECESSARY: This is the entry point for a timed task. It records the start time.
ITT_EXTERN_C void ITTAPI __itt_task_begin(const __itt_domain *domain, __itt_id taskid, __itt_id parentid, __itt_string_handle *name) {
  if (!UniController::IsCollectionEnabled()) {
    return;
  }
  
  // The only enabled feature is CclSummary, so we check for that.
  if (!itt_collector->IsCclSummaryOn()) {
    return;
  }

  ThreadTaskDescriptor desc;
#ifdef _WIN32
  if (domain && domain->nameA) {
    strncpy_s(desc.domain, sizeof(desc.domain), domain->nameA, sizeof(desc.domain) - 2);
  } else {
    desc.domain[0] = 0;
  }
  if (name && name->strA) {
    strncpy_s(desc.name, sizeof(desc.name), name->strA, sizeof(desc.name) - 2);
  } else {
    desc.name[0] = 0;
  }