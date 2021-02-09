/*
 * Copyright (C) 2016 The Android Open Source Project
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

#include "android-base/properties.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#if !defined(_WIN32)
using namespace std::literals;
#endif

TEST(properties, smoke) {
  android::base::SetProperty("debug.libbase.property_test", "hello");

  std::string s = android::base::GetProperty("debug.libbase.property_test", "");
  ASSERT_EQ("hello", s);

  android::base::SetProperty("debug.libbase.property_test", "world");
  s = android::base::GetProperty("debug.libbase.property_test", "");
  ASSERT_EQ("world", s);

  s = android::base::GetProperty("this.property.does.not.exist", "");
  ASSERT_EQ("", s);

  s = android::base::GetProperty("this.property.does.not.exist", "default");
  ASSERT_EQ("default", s);
}

TEST(properties, empty) {
  // Because you can't delete a property, people "delete" them by
  // setting them to the empty string. In that case we'd want to
  // keep the default value (like cutils' property_get did).
  android::base::SetProperty("debug.libbase.property_test", "");
  std::string s = android::base::GetProperty("debug.libbase.property_test", "default");
  ASSERT_EQ("default", s);
}

static void CheckGetBoolProperty(bool expected, const std::string& value, bool default_value) {
  android::base::SetProperty("debug.libbase.property_test", value.c_str());
  ASSERT_EQ(expected, android::base::GetBoolProperty("debug.libbase.property_test", default_value));
}

TEST(properties, GetBoolProperty_true) {
  CheckGetBoolProperty(true, "1", false);
  CheckGetBoolProperty(true, "y", false);
  CheckGetBoolProperty(true, "yes", false);
  CheckGetBoolProperty(true, "on", false);
  CheckGetBoolProperty(true, "true", false);
}

TEST(properties, GetBoolProperty_false) {
  CheckGetBoolProperty(false, "0", true);
  CheckGetBoolProperty(false, "n", true);
  CheckGetBoolProperty(false, "no", true);
  CheckGetBoolProperty(false, "off", true);
  CheckGetBoolProperty(false, "false", true);
}

TEST(properties, GetBoolProperty_default) {
  CheckGetBoolProperty(true, "burp", true);
  CheckGetBoolProperty(false, "burp", false);
}

template <typename T> void CheckGetIntProperty() {
  // Positive and negative.
  android::base::SetProperty("debug.libbase.property_test", "-12");
  EXPECT_EQ(T(-12), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));

  // Default value.
  android::base::SetProperty("debug.libbase.property_test", "");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45));

  // Bounds checks.
  android::base::SetProperty("debug.libbase.property_test", "0");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "1");
  EXPECT_EQ(T(1), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "2");
  EXPECT_EQ(T(2), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
  android::base::SetProperty("debug.libbase.property_test", "3");
  EXPECT_EQ(T(45), android::base::GetIntProperty<T>("debug.libbase.property_test", 45, 1, 2));
}

template <typename T> void CheckGetUintProperty() {
  // Positive.
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetUintProperty<T>("debug.libbase.property_test", 45));

  // Default value.
  android::base::SetProperty("debug.libbase.property_test", "");
  EXPECT_EQ(T(45), android::base::GetUintProperty<T>("debug.libbase.property_test", 45));

  // Bounds checks.
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(12), android::base::GetUintProperty<T>("debug.libbase.property_test", 33, 22));
  android::base::SetProperty("debug.libbase.property_test", "12");
  EXPECT_EQ(T(5), android::base::GetUintProperty<T>("debug.libbase.property_test", 5, 10));
}

TEST(properties, GetIntProperty_int8_t) { CheckGetIntProperty<int8_t>(); }
TEST(properties, GetIntProperty_int16_t) { CheckGetIntProperty<int16_t>(); }
TEST(properties, GetIntProperty_int32_t) { CheckGetIntProperty<int32_t>(); }
TEST(properties, GetIntProperty_int64_t) { CheckGetIntProperty<int64_t>(); }

TEST(properties, GetUintProperty_uint8_t) { CheckGetUintProperty<uint8_t>(); }
TEST(properties, GetUintProperty_uint16_t) { CheckGetUintProperty<uint16_t>(); }
TEST(properties, GetUintProperty_uint32_t) { CheckGetUintProperty<uint32_t>(); }
TEST(properties, GetUintProperty_uint64_t) { CheckGetUintProperty<uint64_t>(); }

TEST(properties, WaitForProperty) {
#if defined(__BIONIC__)
  std::atomic<bool> flag{false};
  std::thread thread([&]() {
    std::this_thread::sleep_for(100ms);
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "a");
    while (!flag) std::this_thread::yield();
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "b");
  });

  ASSERT_TRUE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "a", 1s));
  flag = true;
  ASSERT_TRUE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "b", 1s));
  thread.join();
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}

TEST(properties, WaitForProperty_timeout) {
#if defined(__BIONIC__)
  auto t0 = std::chrono::steady_clock::now();
  ASSERT_FALSE(android::base::WaitForProperty("debug.libbase.WaitForProperty_timeout_test", "a",
                                              200ms));
  auto t1 = std::chrono::steady_clock::now();

  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0), 200ms);
  // Upper bounds on timing are inherently flaky, but let's try...
  ASSERT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0), 600ms);
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}

TEST(properties, WaitForProperty_MaxTimeout) {
#if defined(__BIONIC__)
  std::atomic<bool> flag{false};
  std::thread thread([&]() {
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "a");
    while (!flag) std::this_thread::yield();
    std::this_thread::sleep_for(500ms);
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "b");
  });

  ASSERT_TRUE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "a", 1s));
  flag = true;
  // Test that this does not immediately return false due to overflow issues with the timeout.
  ASSERT_TRUE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "b"));
  thread.join();
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}

TEST(properties, WaitForProperty_NegativeTimeout) {
#if defined(__BIONIC__)
  std::atomic<bool> flag{false};
  std::thread thread([&]() {
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "a");
    while (!flag) std::this_thread::yield();
    std::this_thread::sleep_for(500ms);
    android::base::SetProperty("debug.libbase.WaitForProperty_test", "b");
  });

  ASSERT_TRUE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "a", 1s));
  flag = true;
  // Assert that this immediately returns with a negative timeout
  ASSERT_FALSE(android::base::WaitForProperty("debug.libbase.WaitForProperty_test", "b", -100ms));
  thread.join();
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}

TEST(properties, WaitForPropertyCreation) {
#if defined(__BIONIC__)
  std::thread thread([&]() {
    std::this_thread::sleep_for(100ms);
    android::base::SetProperty("debug.libbase.WaitForPropertyCreation_test", "a");
  });

  ASSERT_TRUE(android::base::WaitForPropertyCreation(
          "debug.libbase.WaitForPropertyCreation_test", 1s));
  thread.join();
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}

TEST(properties, WaitForPropertyCreation_timeout) {
#if defined(__BIONIC__)
  auto t0 = std::chrono::steady_clock::now();
  ASSERT_FALSE(android::base::WaitForPropertyCreation(
          "debug.libbase.WaitForPropertyCreation_timeout_test", 200ms));
  auto t1 = std::chrono::steady_clock::now();

  ASSERT_GE(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0), 200ms);
  // Upper bounds on timing are inherently flaky, but let's try...
  ASSERT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0), 600ms);
#else
  GTEST_LOG_(INFO) << "This test does nothing on the host.\n";
#endif
}
