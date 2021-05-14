/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include <dlfcn.h>
#include <inttypes.h>
#include <signal.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <android-base/stringprintf.h>

#include <unwindstack/LocalUnwinder.h>

namespace unwindstack {

static std::vector<LocalFrameData>* g_frame_info;
static LocalUnwinder* g_unwinder;

extern "C" void SignalLocalInnerFunction() {
  g_unwinder->Unwind(g_frame_info, 256);
}

extern "C" void SignalLocalMiddleFunction() {
  SignalLocalInnerFunction();
}

extern "C" void SignalLocalOuterFunction() {
  SignalLocalMiddleFunction();
}

static void SignalLocalCallerHandler(int, siginfo_t*, void*) {
  SignalLocalOuterFunction();
}

static std::string ErrorMsg(const std::vector<const char*>& function_names,
                            const std::vector<LocalFrameData>& frame_info) {
  std::string unwind;
  size_t i = 0;
  for (const auto& frame : frame_info) {
    unwind += android::base::StringPrintf("#%02zu pc 0x%" PRIx64 " rel_pc 0x%" PRIx64, i++,
                                          frame.pc, frame.rel_pc);
    if (frame.map_info != nullptr) {
      if (!frame.map_info->name.empty()) {
        unwind += " " + frame.map_info->name;
      } else {
        unwind += android::base::StringPrintf(" 0x%" PRIx64 "-0x%" PRIx64, frame.map_info->start,
                                              frame.map_info->end);
      }
      if (frame.map_info->offset != 0) {
        unwind += android::base::StringPrintf(" offset 0x%" PRIx64, frame.map_info->offset);
      }
    }
    if (!frame.function_name.empty()) {
      unwind += " " + frame.function_name;
      if (frame.function_offset != 0) {
        unwind += android::base::StringPrintf("+%" PRId64, frame.function_offset);
      }
    }
    unwind += '\n';
  }

  return std::string(
             "Unwind completed without finding all frames\n"
             "  Looking for function: ") +
         function_names.front() + "\n" + "Unwind data:\n" + unwind;
}

// This test assumes that this code is compiled with optimizations turned
// off. If this doesn't happen, then all of the calls will be optimized
// away.
extern "C" void LocalInnerFunction(LocalUnwinder* unwinder, bool unwind_through_signal) {
  std::vector<LocalFrameData> frame_info;
  g_frame_info = &frame_info;
  g_unwinder = unwinder;
  std::vector<const char*> expected_function_names;

  if (unwind_through_signal) {
    struct sigaction act, oldact;
    memset(&act, 0, sizeof(act));
    act.sa_sigaction = SignalLocalCallerHandler;
    act.sa_flags = SA_RESTART | SA_ONSTACK;
    ASSERT_EQ(0, sigaction(SIGUSR1, &act, &oldact));

    raise(SIGUSR1);

    ASSERT_EQ(0, sigaction(SIGUSR1, &oldact, nullptr));

    expected_function_names = {"LocalOuterFunction",        "LocalMiddleFunction",
                               "LocalInnerFunction",        "SignalLocalOuterFunction",
                               "SignalLocalMiddleFunction", "SignalLocalInnerFunction"};
  } else {
    ASSERT_TRUE(unwinder->Unwind(&frame_info, 256));

    expected_function_names = {"LocalOuterFunction", "LocalMiddleFunction", "LocalInnerFunction"};
  }

  for (auto& frame : frame_info) {
    if (frame.function_name == expected_function_names.back()) {
      expected_function_names.pop_back();
      if (expected_function_names.empty()) {
        break;
      }
    }
  }

  ASSERT_TRUE(expected_function_names.empty()) << ErrorMsg(expected_function_names, frame_info);
}

extern "C" void LocalMiddleFunction(LocalUnwinder* unwinder, bool unwind_through_signal) {
  LocalInnerFunction(unwinder, unwind_through_signal);
}

extern "C" void LocalOuterFunction(LocalUnwinder* unwinder, bool unwind_through_signal) {
  LocalMiddleFunction(unwinder, unwind_through_signal);
}

class LocalUnwinderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    unwinder_.reset(new LocalUnwinder);
    ASSERT_TRUE(unwinder_->Init());
  }

  std::unique_ptr<LocalUnwinder> unwinder_;
};

TEST_F(LocalUnwinderTest, local) {
  LocalOuterFunction(unwinder_.get(), false);
}

TEST_F(LocalUnwinderTest, local_signal) {
  LocalOuterFunction(unwinder_.get(), true);
}

TEST_F(LocalUnwinderTest, local_multiple) {
  ASSERT_NO_FATAL_FAILURE(LocalOuterFunction(unwinder_.get(), false));

  ASSERT_NO_FATAL_FAILURE(LocalOuterFunction(unwinder_.get(), true));

  ASSERT_NO_FATAL_FAILURE(LocalOuterFunction(unwinder_.get(), false));

  ASSERT_NO_FATAL_FAILURE(LocalOuterFunction(unwinder_.get(), true));
}

// This test verifies that doing an unwind before and after a dlopen
// works. It's verifying that the maps read during the first unwind
// do not cause a problem when doing the unwind using the code in
// the dlopen'd code.
TEST_F(LocalUnwinderTest, unwind_after_dlopen) {
  // Prime the maps data.
  ASSERT_NO_FATAL_FAILURE(LocalOuterFunction(unwinder_.get(), false));

  std::string testlib(testing::internal::GetArgvs()[0]);
  auto const value = testlib.find_last_of('/');
  if (value == std::string::npos) {
    testlib = "../";
  } else {
    testlib = testlib.substr(0, value + 1) + "../";
  }
  testlib += "libunwindstack_local.so";

  void* handle = dlopen(testlib.c_str(), RTLD_NOW);
  ASSERT_TRUE(handle != nullptr);

  void (*unwind_function)(void*, void*) =
      reinterpret_cast<void (*)(void*, void*)>(dlsym(handle, "TestlibLevel1"));
  ASSERT_TRUE(unwind_function != nullptr);

  std::vector<LocalFrameData> frame_info;
  unwind_function(unwinder_.get(), &frame_info);

  ASSERT_EQ(0, dlclose(handle));

  std::vector<const char*> expected_function_names{"TestlibLevel1", "TestlibLevel2",
                                                   "TestlibLevel3", "TestlibLevel4"};

  for (auto& frame : frame_info) {
    if (frame.function_name == expected_function_names.back()) {
      expected_function_names.pop_back();
      if (expected_function_names.empty()) {
        break;
      }
    }
  }

  ASSERT_TRUE(expected_function_names.empty()) << ErrorMsg(expected_function_names, frame_info);
}

}  // namespace unwindstack
