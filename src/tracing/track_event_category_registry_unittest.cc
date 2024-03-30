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

#include "perfetto/tracing/track_event.h"
#include "test/gtest_and_gmock.h"

PERFETTO_DEFINE_CATEGORIES(perfetto::Category("test"));
PERFETTO_TRACK_EVENT_STATIC_STORAGE();

static bool callback_called = false;

static void callback(uint32_t instances, size_t client_index) {
  callback_called = true;
  EXPECT_EQ(instances, (uint32_t)1);
}

TEST(TrackEventCategoryRegistryTest, CategorySetupOnInit) {
  auto category = perfetto::internal::kCategoryRegistry.GetCategory(0);
  EXPECT_EQ(category->name, nullptr);
  // Init copies all constexpr categories into our non-const category
  // storage.
  perfetto::internal::kCategoryRegistry.Init();
  EXPECT_EQ(category->name, "test");
}

TEST(TrackEventCategoryRegistryTest, InstancesCallback) {
  auto category = perfetto::internal::kCategoryRegistry.GetCategory(0);
  category->instances_callback = callback;
  callback_called = false;
  perfetto::internal::kCategoryRegistry.EnableCategoryForInstance(0, 0);
  EXPECT_TRUE(callback_called);
}
