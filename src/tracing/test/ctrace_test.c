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
#include "perfetto/base/build_config.h"
#include "stdatomic.h"

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)
#include <io.h>
#else
#include <unistd.h>
#endif

#define MY_CATEGORIES(C)        \
  C(cat1, "cat1",  "category 1", {NULL}) \
  C(cat2, "cat2", "category 2", {"mytag"})

CTRACE_DECLARE_CATEGORIES(MY_CATEGORIES)
CTRACE_DEFINE_CATEGORIES(MY_CATEGORIES)

#if PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

#include "Windows.h"

static void sleep(unsigned interval_us) {
  // The Windows Sleep function takes a millisecond count. Round up so that
  // short sleeps don't turn into a busy wait. Note that the sleep granularity
  // on Windows can dynamically vary from 1 ms to ~16 ms, so don't count on this
  // being a short sleep.
  Sleep(((interval_us + 999) / 1000));
}

#else  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

static void sleep(unsigned interval_us) {
  ::usleep(static_cast<useconds_t>(interval_us));
}

#endif  // PERFETTO_BUILDFLAG(PERFETTO_OS_WIN)

int main(void) {
  ctrace_trace_session_handle handle;
  struct ctrace_trace_config cfg;
  
  ctrace_init_args init_args = {.api_version = CTRACE_API_VERSION,
               .backend = CTRACE_IN_PROCESS_BACKEND};
  ctrace_register();
  ctrace_init(&init_args);


  cfg.duration_ms = 0;// Explicit stop. Also we don't have a wait() yet.
  cfg.buffer_size_kb = 131072;

  // This code is supposed to generate a valid trace that looks like
  // https://screenshot.googleplex.com/99cvjbnZXc9AmnQ .
  // This executable does not verify the output example.pftrace file, it must
  // be loaded into ui.perfetto.dev and checked manually.

  handle = ctrace_trace_start(&cfg);
  CTRACE_EVENT_INSTANT(cat1, "start");
  for (int i = 0; i < 3; i++) {
    CTRACE_COUNTER(cat1, "iteration", (int64_t)i);
    CTRACE_EVENT_BEGIN(cat1, "event 1");
    sleep(1000);
    CTRACE_EVENT_BEGIN(cat2, "event 2");

    sleep(10000);

    CTRACE_EVENT_END(cat2);
    sleep(1000);
    CTRACE_EVENT_END(cat1);

    sleep(10000);
  }
  CTRACE_EVENT_INSTANT(cat1, "end");
  ctrace_trace_stop(handle, "example.pftrace");
}
