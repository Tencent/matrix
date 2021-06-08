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

#include "android-base/quick_exit.h"

#include <gtest/gtest.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <string>

#include "android-base/test_utils.h"

// These tests are a bit sketchy, since each test run adds global state that affects subsequent
// tests (including ones not in this file!). Exit with 0 in Exiter and stick the at_quick_exit test
// at the end to hack around this.
struct Exiter {
  ~Exiter() {
    _Exit(0);
  }
};

TEST(quick_exit, smoke) {
  ASSERT_EXIT(android::base::quick_exit(123), testing::ExitedWithCode(123), "");
}

TEST(quick_exit, skip_static_destructors) {
  static Exiter exiter;
  ASSERT_EXIT(android::base::quick_exit(123), testing::ExitedWithCode(123), "");
}

TEST(quick_exit, at_quick_exit) {
  static int counter = 4;
  // "Functions passed to at_quick_exit are called in reverse order of their registration."
  ASSERT_EQ(0, android::base::at_quick_exit([]() { _exit(counter); }));
  ASSERT_EQ(0, android::base::at_quick_exit([]() { counter += 2; }));
  ASSERT_EQ(0, android::base::at_quick_exit([]() { counter *= 10; }));
  ASSERT_EXIT(android::base::quick_exit(123), testing::ExitedWithCode(42), "");
}
