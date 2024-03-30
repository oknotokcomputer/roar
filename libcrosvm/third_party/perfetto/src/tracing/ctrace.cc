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

#include "perfetto/tracing/ctrace.h"

#include <stdio.h>

#include <atomic>
#include <fstream>

// Use a dedicated namespace to avoid colliding with other usages of the
// perfetto SDK (which, by default, uses the "perfetto" namespace) in the same
// linker unit. Note: the SDK supports a single namespace, can't do ::foo::bar.
#define PERFETTO_TRACK_EVENT_NAMESPACE ctrace

#include "perfetto/tracing/core/trace_config.h"
#include "perfetto/tracing/tracing.h"
#include "perfetto/tracing/track_event.h"
#include "protos/perfetto/trace/clock_snapshot.pbzero.h"

namespace PERFETTO_TRACK_EVENT_NAMESPACE {
namespace internal {
namespace {

// ::perfetto::Category initializer requires a const char* name. That makes it
// impossible to create an array of N categories without knowing their names
// upfront. Here we override it just for the sake of having a default ctor,
// without polluting that to the base Category class.
struct EmptyCategory : public ::perfetto::Category {
  constexpr EmptyCategory() : ::perfetto::Category("") {}
};

constexpr size_t kMaxCategories = 255;
size_t g_num_categories = 0;
EmptyCategory g_categories[kMaxCategories]{};
std::atomic<uint8_t> g_category_state_storage[kMaxCategories];

// Must be called kCategoryRegistry to reuse the macros below.
::perfetto::internal::TrackEventCategoryRegistry kCategoryRegistry(
    kMaxCategories,
    0,
    nullptr,
    &g_categories[0],
    &g_category_state_storage[0]);

}  // namespace
}  // namespace internal

PERFETTO_INTERNAL_DECLARE_TRACK_EVENT_DATA_SOURCE();
PERFETTO_INTERNAL_DEFINE_TRACK_EVENT_DATA_SOURCE();

}  // namespace PERFETTO_TRACK_EVENT_NAMESPACE

PERFETTO_DECLARE_DATA_SOURCE_STATIC_MEMBERS(
    PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent,
    perfetto::internal::TrackEventDataSourceTraits);

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS(
    PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent,
    perfetto::internal::TrackEventDataSourceTraits);

namespace PERFETTO_TRACK_EVENT_NAMESPACE {
namespace internal {

extern "C" CPERFETTO_EXPORT void ctrace_init(const ctrace_init_args* args) {
  printf("Initializing. Caller API version: %d\n", args->api_version);
  perfetto::TracingInitArgs cpp_args;
  cpp_args.backends = args->backend;
  cpp_args.shmem_size_hint_kb = 0;
  cpp_args.shmem_page_size_hint_kb = 0;
  cpp_args.shmem_batch_commits_duration_ms = 10;
  perfetto::Tracing::Initialize(cpp_args);
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::Register();
}

extern "C" CPERFETTO_EXPORT uint64_t
ctrace_register_categories(const ctrace_category* const* c_cats, uint64_t max) {
  // TODO (colindr b/196630849): This "new std::mutex" is used to get around a
  //  "declaration requires an exit-time destructor" compiler error, but this
  //  doesn't seem any safer. But the compiler is ok with it, seems to have
  //  something to do with trivial vs non-trivial destructors.
  static std::mutex* mutex = new std::mutex;
  std::lock_guard<std::mutex> guard(*mutex);
  size_t base = g_num_categories;
  for (size_t count = 0; count < max; ++count) {
    const ctrace_category* c_cat = c_cats[count];
    ::perfetto::Category& cat = g_categories[g_num_categories++];
    cat.name = c_cat->name;
    cat.instances_callback = c_cat->instances_callback;
    cat.client_index = c_cat->client_index;
    cat.description = c_cat->description;
    for (size_t i = 0; i < cat.tags.size(); i++) {
      cat.tags[i] = c_cat->tags[i];
    }
  }
  kCategoryRegistry.SetCount(g_num_categories);
  return base;
}

extern "C" CPERFETTO_EXPORT void ctrace_add_clock_snapshot(
    struct ctrace_clock_snapshot* snapshot) {
  using ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent;
  TrackEvent::Trace([snapshot](TrackEvent::TraceContext ctx) {
    auto packet = ctx.NewTracePacket();
    packet->set_timestamp_clock_id(TrackEvent::GetTraceClockId());
    packet->set_timestamp(TrackEvent::GetTraceTimeNs());
    auto* clock_snapshot = packet->set_clock_snapshot();
    for (int i = 0; i < 2; i++) {
      auto* clock = clock_snapshot->add_clocks();
      clock->set_clock_id(snapshot->clocks[i].clock_id);
      clock->set_timestamp(snapshot->clocks[i].timestamp);
      clock->set_is_incremental(snapshot->clocks[i].is_incremental);
      if (snapshot->clocks[i].unit_multiplier_ns != 0) {
        clock->set_unit_multiplier_ns(snapshot->clocks[i].unit_multiplier_ns);
      }
    }
  });
}

extern "C" CPERFETTO_EXPORT void trace_event_begin(size_t category_index,
                                                   uint32_t instances,
                                                   const char* name) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, perfetto::StaticString(name),
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_SLICE_BEGIN);
}

extern "C" CPERFETTO_EXPORT void trace_event_end(size_t category_index,
                                                 uint32_t instances) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, nullptr,
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_SLICE_END);
}

extern "C" CPERFETTO_EXPORT void trace_event_instant(size_t category_index,
                                                     uint32_t instances,
                                                     const char* name) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, perfetto::StaticString(name),
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_INSTANT);
}

extern "C" CPERFETTO_EXPORT uint64_t trace_create_async(uint64_t category_index,
                                                        uint32_t instances,
                                                        const char* name) {
    uint64_t flow_id = perfetto::FlowIdGenerator::generate_unique_flow_id();
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, perfetto::StaticString(name),
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_INSTANT,
      perfetto::Flow::ProcessScoped(flow_id));
    return flow_id;
}


extern "C" CPERFETTO_EXPORT void trace_begin_async(size_t category_index,
                                                  uint32_t instances,
                                                   const char * name,
                                                   uint64_t terminating_flow_id) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, perfetto::StaticString(name),
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_SLICE_BEGIN,
      perfetto::TerminatingFlow::ProcessScoped(terminating_flow_id));
}

extern "C" CPERFETTO_EXPORT uint64_t trace_pause_async(uint64_t category_index,
                                          uint32_t instances) {
    uint64_t flow_id = perfetto::FlowIdGenerator::generate_unique_flow_id();
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, nullptr,
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_SLICE_END,
      perfetto::Flow::ProcessScoped(flow_id), "is_ready", false);
    return flow_id;
}

extern "C" CPERFETTO_EXPORT void trace_end_async(uint64_t category_index,
                                                    uint32_t instances) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, nullptr,
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_SLICE_END,
      "is_ready", true);
}

extern "C" CPERFETTO_EXPORT void trace_counter(size_t category_index,
                                               uint32_t instances,
                                               const char* track,
                                               int64_t value) {
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::TraceForCategory(
      instances, category_index, nullptr,
      perfetto::protos::pbzero::TrackEvent::Type::TYPE_COUNTER,
      ::perfetto::CounterTrack(track), value);
}

extern "C" CPERFETTO_EXPORT ctrace_trace_session_handle
ctrace_trace_start(const ctrace_trace_config* c_cfg) {
  ::perfetto::TraceConfig cfg;
  cfg.add_buffers()->set_size_kb(c_cfg->buffer_size_kb);
  if (c_cfg->duration_ms != 0) {
    cfg.set_duration_ms(c_cfg->duration_ms);
  }
  auto* ds_cfg = cfg.add_data_sources()->mutable_config();
  // TODO (colindr b/196630849): The ctrace API only supports track_events,
  //  but other producers on the system backend might produce other
  //  datasources which the caller of this function may want to capture.
  //  We should add fields on ctrace_trace_config to support this.
  ds_cfg->set_name("track_event");

  auto tracing_session = perfetto::Tracing::NewTrace();
  // TODO (colindr b/196630849): add an optional output path to which a timed
  //  trace will be written.
  tracing_session->Setup(cfg);
  tracing_session->StartBlocking();
  return static_cast<ctrace_trace_session_handle>(tracing_session.release());
}

extern "C" CPERFETTO_EXPORT ctrace_trace_session_handle
ctrace_trace_start_from_config_proto(void* const config_ptr,
                                     uint64_t config_len) {
  ::perfetto::TraceConfig cfg;
  if (!cfg.ParseFromArray(config_ptr, config_len)) {
    return nullptr;
  }

  auto tracing_session = perfetto::Tracing::NewTrace();
  tracing_session->Setup(cfg);
  tracing_session->StartBlocking();
  return static_cast<ctrace_trace_session_handle>(tracing_session.release());
}


extern "C" CPERFETTO_EXPORT void ctrace_trace_stop(
    ctrace_trace_session_handle handle,
    const char* path) {
  std::unique_ptr<perfetto::TracingSession> tracing_session(
      static_cast<perfetto::TracingSession*>(handle));
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::Flush();
  tracing_session->StopBlocking();
  std::vector<char> trace_data(tracing_session->ReadTraceBlocking());

  // Write the result into a file. If path is null we assume the path output was
  // configured at the start of the trace.
  if (path != nullptr) {
    std::ofstream output;
    output.open(path, std::ios::out | std::ios::binary);
    output.write(&trace_data[0], static_cast<long>(trace_data.size()));
    output.close();
  }
}

extern "C" CPERFETTO_EXPORT ctrace_trace_buffer ctrace_trace_stop_to_buffer(
    ctrace_trace_session_handle handle) {
  std::unique_ptr<perfetto::TracingSession> tracing_session(
      static_cast<perfetto::TracingSession*>(handle));
  ::PERFETTO_TRACK_EVENT_NAMESPACE::TrackEvent::Flush();
  tracing_session->StopBlocking();

  // Ownership of this vector is transferred to the caller.
  std::vector<char>* trace_data_vec = new std::vector<char>(
      tracing_session->ReadTraceBlocking());
  ctrace_trace_buffer trace_data;
  trace_data.std_vec = trace_data_vec;
  trace_data.data = trace_data_vec->data();
  trace_data.size = trace_data_vec->size();
  return trace_data;
}

extern "C" CPERFETTO_EXPORT void ctrace_free_trace_buffer(
    ctrace_trace_buffer* const trace_data) {
  delete reinterpret_cast<std::vector<char>*>(trace_data->std_vec);
  trace_data->std_vec = nullptr;
  trace_data->data = nullptr;
  trace_data->size = 0;
}

}  // namespace internal
}  // namespace PERFETTO_TRACK_EVENT_NAMESPACE
