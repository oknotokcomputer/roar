/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef INCLUDE_PERFETTO_TRACING_CTRACE_H_
#define INCLUDE_PERFETTO_TRACING_CTRACE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_load_explicit;
using std::atomic_uint_fast32_t;
using std::atomic_uint_fast64_t;
using std::memory_order_relaxed;
#else
#if !defined(_WIN32)
#include "stdatomic.h"
#endif
#endif

#if defined(_MSC_VER)
    //  Microsoft
    #define CPERFETTO_EXPORT __declspec(dllexport)
#elif defined(__GNUC__)
    //  GCC
    #define CPERFETTO_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif
// TODO (colindr b/196630849): this is copied from backend_type.h, I'm
//  assuming we need it copied here so clients can just have a copy of
//  ctrace.h for building.
enum BackendType {
  CTRACE_UNSPECIFIED_BACKEND = 0,

  // Connects to a previously-initialized perfetto tracing backend for
  // in-process. If the in-process backend has not been previously initialized
  // it will do so and create the tracing service on a dedicated thread.
  CTRACE_IN_PROCESS_BACKEND = 1 << 0,

  // Connects to the system tracing service (e.g. on Linux/Android/Mac uses a
  // named UNIX socket).
  CTRACE_SYSTEM_BACKEND = 1 << 1,
};

// Category definition.
typedef struct ctrace_category {
  uint64_t client_index;
  void (*instances_callback)(uint32_t instances, uint64_t client_index);
  const char* name;
  const char* description;
  // tags are a single comma-separated string
  const char* tags[4];
} ctrace_category;

// Clock, used by a ctrace_clock_snapshot.
typedef struct ctrace_clock {
  uint32_t clock_id;
  uint64_t timestamp;
  bool is_incremental;
  uint64_t unit_multiplier_ns;
} ctrace_clock;

// Clock snapshot struct, only supports two clocks at a time so we can
// avoid doing a struct hack.
typedef struct ctrace_clock_snapshot {
  struct ctrace_clock clocks[2];
} ctrace_clock_snapshot;

// Registers a list of categories.
// NOTE: This should be callable multiple times before or after
// initialization. Some executables will have N independent sources all
// calling into this, without being aware of each other.
// Args: an array of ctrace_category pointers and the size of the
// array.
CPERFETTO_EXPORT uint64_t
ctrace_register_categories(const struct ctrace_category* const* c_cats,
                           uint64_t max);

// Add a clock snapshot for exactly two clocks to the current trace.
CPERFETTO_EXPORT void ctrace_add_clock_snapshot(
    struct ctrace_clock_snapshot* snapshot);

#define CTRACE_API_VERSION 1
typedef struct ctrace_init_args {
  uint32_t api_version;
  uint32_t backend;
  uint32_t shmem_size_hint_kb;
  uint32_t shmem_page_size_hint_kb;
  uint32_t shmem_batch_commits_duration_ms;
} ctrace_init_args;

CPERFETTO_EXPORT void ctrace_init(const struct ctrace_init_args*);

CPERFETTO_EXPORT void trace_event_begin(uint64_t category_index,
                                        uint32_t instances,
                                        const char* name);

CPERFETTO_EXPORT void trace_event_end(uint64_t category_index,
                                      uint32_t instances);

CPERFETTO_EXPORT void trace_event_instant(uint64_t category_index,
                                          uint32_t instances,
                                          const char* name);

CPERFETTO_EXPORT void trace_counter(uint64_t category_index,
                                    uint32_t instances,
                                    const char* track,
                                    int64_t value);

CPERFETTO_EXPORT uint64_t trace_create_async(uint64_t category_index,
                                             uint32_t instances,
                                             const char* name);

CPERFETTO_EXPORT void trace_begin_async(uint64_t category_index,
                                          uint32_t instances,
                                          const char* name,
                                          uint64_t terminating_flow_id);

CPERFETTO_EXPORT uint64_t trace_pause_async(uint64_t category_index,
                                          uint32_t instances);

CPERFETTO_EXPORT void trace_end_async(uint64_t category_index,
                        uint32_t instances);

struct ctrace_trace_config {
  uint32_t duration_ms;
  uint32_t buffer_size_kb;
};

typedef void* ctrace_trace_session_handle;

CPERFETTO_EXPORT ctrace_trace_session_handle
ctrace_trace_start(const struct ctrace_trace_config*);

// Starts a trace using the provided TraceConfig proto.
// Returns a valid trace session handle on success, or nullptr on failure.
CPERFETTO_EXPORT ctrace_trace_session_handle
ctrace_trace_start_from_config_proto(void* const, uint64_t);

CPERFETTO_EXPORT void ctrace_trace_stop(ctrace_trace_session_handle,
                                        const char*);

// Contains a trace returned by ctrace_trace_stop_to_buffer.
// The raw trace is stored at `data`, and is of length `size`.
// The std_vec field is private and should not be used by callers.
//
// This structure must be freed by calling `ctrace_free_trace_buffer`.
typedef struct ctrace_trace_buffer {
  void* std_vec;
  void* data;
  uint64_t size;
} ctrace_trace_buffer;

CPERFETTO_EXPORT void ctrace_free_trace_buffer(ctrace_trace_buffer* const);

// Stops the provided trace and returns a `ctrace_trace_buffer` with the vector
// that holds the trace data. This buffer MUST be freed by calling
// `ctrace_free_trace_buffer`.
CPERFETTO_EXPORT ctrace_trace_buffer ctrace_trace_stop_to_buffer(
    ctrace_trace_session_handle);

#ifdef __cplusplus
}
#endif

// -----------------------------------------------------------------------------
// Macros
// Everything starting with _UNDERSCORE is be private / impl details.
// -----------------------------------------------------------------------------

// Track-event-equivalent macros.

#define PERFETTO_INTERNAL_CONCAT2(a, b) a##b
#define PERFETTO_INTERNAL_CONCAT(a, b) PERFETTO_INTERNAL_CONCAT2(a, b)
#define PERFETTO_UID(prefix) PERFETTO_INTERNAL_CONCAT(prefix, __LINE__)

#define _CTRACE_TYPE_SLICE_BEGIN 1
#define _CTRACE_TYPE_SLICE_END 2

#define CTRACE_EVENT_BEGIN_IDX(idx, name)                                    \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      trace_event_begin(                                                     \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances, name);                                                  \
    }                                                                        \
  } while (0)

#define CTRACE_EVENT_END_IDX(idx)                                            \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      trace_event_end(                                                       \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances);                                                        \
    }                                                                        \
  } while (0)

#define CTRACE_EVENT_INSTANT_IDX(idx, name)                                  \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      trace_event_instant(                                                   \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances, name);                                                  \
    }                                                                        \
  } while (0)

// TODO (colindr b/196630849): for floating point counters we'll need a
//  separate macro
#define CTRACE_COUNTER_IDX(idx, track, value)                                \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0)                                                      \
      trace_counter(                                                         \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances, track, value);                                          \
  } while (0)

#define CTRACE_ASYNC_EVENT_CREATE_IDX(idx, name, out_flow_id)                \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      out_flow_id = trace_create_async(                                       \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances, name);                                                        \
    } else {                                                                 \
      out_flow_id = 0;                                                       \
    }                                                                        \
  } while (0)


#define CTRACE_ASYNC_EVENT_BEGIN_IDX(idx, name, terminating_flow_id)        \
  do {                                                                         \
    const struct ctrace_category* cat = catlist[idx];                          \
    uint32_t instances = atomic_load_explicit(                                 \
        &category_state_storage[cat->client_index], memory_order_relaxed);     \
    if (instances != 0) {                                                      \
        trace_begin_async(                                                  \
            atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
                cat->client_index,                                             \
            instances, name, terminating_flow_id);                             \
    }                                                                          \
  } while (0)

#define CTRACE_ASYNC_EVENT_PAUSE_IDX(idx, out_flow_id)                       \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      out_flow_id = trace_pause_async(                                       \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances);                                                        \
    } else {                                                                 \
      out_flow_id = 0;                                                       \
    }                                                                        \
  } while (0)

#define CTRACE_ASYNC_EVENT_END_IDX(idx)                                \
  do {                                                                       \
    const struct ctrace_category* cat = catlist[idx];                        \
    uint32_t instances = atomic_load_explicit(                               \
        &category_state_storage[cat->client_index], memory_order_relaxed);   \
    if (instances != 0) {                                                    \
      trace_end_async(                                                       \
          atomic_load_explicit(&category_index_base, memory_order_relaxed) + \
              cat->client_index,                                             \
          instances);                                         \
    }                                                                        \
  } while (0)


#define CTRACE_EVENT_BEGIN(cat, name) \
  CTRACE_EVENT_BEGIN_IDX(CAT_IDX_##cat, name)

#define CTRACE_EVENT_END(cat) CTRACE_EVENT_END_IDX(CAT_IDX_##cat)

#define CTRACE_EVENT_INSTANT(cat, name) \
  CTRACE_EVENT_INSTANT_IDX(CAT_IDX_##cat, name)

#define CTRACE_COUNTER(cat, name, value) \
  CTRACE_COUNTER_IDX(CAT_IDX_##cat, name, value)

#ifdef __cplusplus
#include "assert.h"
// If the consumer of this API is actually a C++ application, it can use these
// additional macros that take string arguments. This can be convenient when
// integrating cperfetto into C++ applications that have trace macros that
// already take string categories. This can't be done in C because C doesn't
// have constexpr functions.

// Dumb constexpr string comparison
static constexpr bool string_eq(const char* a, const char* b) {
  return *a != *b ? false : (!*a || !*b) ? (*a == *b) : string_eq(a + 1, b + 1);
}

constexpr size_t idx_for_category_name_impl(const char* name,
                                            size_t index,
                                            size_t max,
                                            const ctrace_category catlist[]) {
  assert(index < max);  // This doesn't work before C++ 14
  return index >= max
             ? max - 1
             : string_eq(catlist[index].name, name)
                   ? index
                   : idx_for_category_name_impl(name, index + 1, max, catlist);
}

constexpr size_t idx_for_category_name(const char* name,
                                       size_t max,
                                       const ctrace_category catlist[]) {
  return idx_for_category_name_impl(name, 0, max, catlist);
}

#define CTRACE_EVENT_BEGIN_STR(cat, name) \
  CTRACE_EVENT_BEGIN_IDX(                 \
      idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), name)

#define CTRACE_EVENT_END_STR(cat) \
  CTRACE_EVENT_END_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist))

#define CTRACE_EVENT_INSTANT_STR(cat, name) \
  CTRACE_EVENT_INSTANT_IDX(                 \
      idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), name)

#define CTRACE_COUNTER_STR(cat, name, value)                                 \
  CTRACE_COUNTER_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), \
                     name, value)

#define CTRACE_ASYNC_EVENT_CREATE_STR(cat, name, out_flow_id)                           \
  CTRACE_ASYNC_EVENT_CREATE_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), \
                                name, out_flow_id)

#define CTRACE_ASYNC_EVENT_BEGIN_STR(cat, name, terminating_flow_id)                   \
  CTRACE_ASYNC_EVENT_BEGIN_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), \
                                name, terminating_flow_id)

#define CTRACE_ASYNC_EVENT_PAUSE_STR(cat, out_flow_id)                                 \
  CTRACE_ASYNC_EVENT_PAUSE_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist), \
                                                     out_flow_id)

#define CTRACE_ASYNC_EVENT_END_STR(cat) \
  CTRACE_ASYNC_EVENT_END_IDX(idx_for_category_name(cat, _CTRACE_NUM_CATS, *catlist))

#endif

// Category definition macros

// The macro below do the following:
//
// User code:
//   #define MY_CATEGORIES(C)          \
//     C(cat1, "category 1", {NULL})       \
//     C(cat2, "catgegory 2", {"debug","othertag"})
//   CTRACE_DEFINE_CATEGORIES(MY_CATEGORIES)
//
// Expanded code:
//   Declarations:
//    enum { CAT_IDX_cat1, CAT_IDX_cat2, _CTRACE_NUM_CATS };
//    extern atomic_uint_fast32_t category_state_storage[_CTRACE_NUM_CATS];
//    extern atomic_uint_fast32_t category_index_base;
//    extern void cperfetto_callback(uint32_t instances, uint64_t client_index);
//    extern void ctrace_register(void);
//    extern struct ctrace_category* catlist[];
//    extern struct ctrace_category cat1, cat2, * _unused_7;
//
//
//  Definitions:
//   struct ctrace_category cat1 = \
//     {0, cperfetto_callback, "cat1", "category 1", {NULL}},
//                          cat2 = \
//     {0, cperfetto_callback, "cat2", "category 2", {"debug","othertag"}},
//                          * _unused_7;   // just for trailing comma.
//
//   struct ctrace_category* catlist[] = {&cat1, &cat2, NULL };
//   atomic_uint_fast32_t category_state_storage[_CTRACE_NUM_CATS];
//   atomic_uint_fast64_t category_index_base = ATOMIC_VAR_INIT(0);
//   void ctrace_register(void) {
//     atomic_store_explicit(
//       &category_index_base,
//       ctrace_register_categories(&catlist[0], _CTRACE_NUM_CATS),
//       memory_order_relaxed
//     );
//   }
//   void cperfetto_callback(uint32_t instances, uint64_t client_index) {
//     atomic_store_explicit(
//       &category_state_storage[client_index],
//       instances, memory_order_relaxed);
//   }

// TODO (colindr b/196630849): this used to use the line number to make it
//  more unique, but I had to remove that so I could split up the declare and
//  define macros. Perhaps there's another way we can safely make sure this
//  macro does not declare or define a variable that might be used elsewhere.
#define _CTRACE_UNUSED _ctrace_unused
#define _CTRACE_CONCAT(a, b) a##b
#define _CTRACE_GEN_ECHO_COMMA(id, name, description, tags) id,
#define _CTRACE_GEN_PTR_COMMA(id, name, description, tags) &id,
#define _CTRACE_GEN_CAT_VAR(id, name, description, tags) \
  id = {CAT_IDX_##id, cperfetto_callback, name, description, tags},

#define _CTRACE_GEN_CAT_DECLARATIONS(DEF)         \
  extern const struct ctrace_category* catlist[]; \
  extern struct ctrace_category DEF(_CTRACE_GEN_ECHO_COMMA) * _CTRACE_UNUSED

#define _CTRACE_GEN_CAT_DEFINITIONS(DEF) \
  struct ctrace_category DEF(_CTRACE_GEN_CAT_VAR) * _CTRACE_UNUSED

#define _CTRACE_GEN_ENUM(id, name, description, tags) \
  _CTRACE_CONCAT(CAT_IDX_, id),
#define _CTRACE_GEN_CAT_INDEXES(DEF) \
  enum { DEF(_CTRACE_GEN_ENUM) _CTRACE_NUM_CATS }

#define CTRACE_DECLARE_CATEGORIES(DEF)                                       \
  _CTRACE_GEN_CAT_INDEXES(DEF);                                              \
  extern atomic_uint_fast32_t category_state_storage[_CTRACE_NUM_CATS];      \
  extern atomic_uint_fast64_t category_index_base;                           \
  extern void cperfetto_callback(uint32_t instances, uint64_t client_index); \
  extern void ctrace_register(void);                                         \
  _CTRACE_GEN_CAT_DECLARATIONS(DEF);

#define CTRACE_DEFINE_CATEGORIES(DEF)                                          \
  _CTRACE_GEN_CAT_DEFINITIONS(DEF);                                            \
  const struct ctrace_category* catlist[] = {DEF(_CTRACE_GEN_PTR_COMMA) NULL}; \
  atomic_uint_fast32_t category_state_storage[_CTRACE_NUM_CATS];               \
  atomic_uint_fast64_t category_index_base = ATOMIC_VAR_INIT(0);               \
  void ctrace_register(void) {                                                 \
    atomic_store_explicit(                                                     \
        &category_index_base,                                                  \
        ctrace_register_categories(&catlist[0], _CTRACE_NUM_CATS),             \
        memory_order_relaxed);                                                 \
  }                                                                            \
  void cperfetto_callback(uint32_t instances, uint64_t client_index) {         \
    atomic_store_explicit(&category_state_storage[client_index], instances,    \
                          memory_order_relaxed);                               \
  }

#endif  // INCLUDE_PERFETTO_TRACING_CTRACE_H_#pragma once
