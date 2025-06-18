
//==============================================================
// Copyright (C) Intel Corporation
//
// SPDX-License-Identifier: MIT
// =============================================================
// Commented by Colin Luangrath

#ifndef PTI_TOOLS_UNITRACE_UNIEVENT_H
#define PTI_TOOLS_UNITRACE_UNIEVENT_H

// Defines the structures and enums used for event tracing in the unitrace tool.
enum EVENT_TYPE {
  EVENT_NULL = 0,
  EVENT_DURATION_START,
  EVENT_DURATION_END,
  EVENT_FLOW_SOURCE,
  EVENT_FLOW_SINK,
  EVENT_COMPLETE,
  EVENT_MARK,
};

// Defines the types of APIs that can be traced.
enum API_TYPE {
  API_TYPE_NONE,
  API_TYPE_MPI,
  API_TYPE_ITT,
  API_TYPE_CCL
};

// Defines the arguments used for MPI events.
typedef struct MpiArgs_ {
  int src_location;
  int src_tag;
  int dst_location;
  int dst_tag;
  size_t src_size;
  size_t dst_size;
  int64_t mpi_counter;
  bool is_tagged;
} MpiArgs;

// Same thing but for ITT
typedef struct IttArgs_ {
  size_t count = 0;
  bool isIndirectData = false;
  int type;
  const char* key;
  struct IttArgs_* next = nullptr;
  void* data[1];
} IttArgs;

// Defines the structure for a host event record, which contains information about an event.
typedef struct HostEventRecord_ {
  uint64_t id_;
  uint64_t start_time_;
  uint64_t end_time_;
  char *name_ = nullptr;
  API_TRACING_ID api_id_;
  EVENT_TYPE type_;

  // The API type for this event
  API_TYPE api_type_ = API_TYPE::API_TYPE_NONE;
  union{
    MpiArgs mpi_args_;
    IttArgs itt_args_;
  };
} HostEventRecord; 

#endif // PTI_TOOLS_UNITRACE_UNIEVENT_H