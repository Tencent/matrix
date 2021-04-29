
/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_STRING_CHARS_H_
#define LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_STRING_CHARS_H_

#include "jni.h"
#include "nativehelper_utils.h"

// A smart pointer that provides access to a jchar* given a JNI jstring.
// Unlike GetStringChars, we throw NullPointerException rather than abort if
// passed a null jstring, and get will return NULL.
// This makes the correct idiom very simple:
//
//   ScopedStringChars name(env, java_name);
//   if (name.get() == NULL) {
//     return NULL;
//   }
class ScopedStringChars {
 public:
  ScopedStringChars(JNIEnv* env, jstring s) : env_(env), string_(s), size_(0) {
    if (s == NULL) {
      chars_ = NULL;
      jniThrowNullPointerException(env, NULL);
    } else {
      chars_ = env->GetStringChars(string_, NULL);
      if (chars_ != NULL) {
        size_ = env->GetStringLength(string_);
      }
    }
  }

  ~ScopedStringChars() {
    if (chars_ != NULL) {
      env_->ReleaseStringChars(string_, chars_);
    }
  }

  const jchar* get() const {
    return chars_;
  }

  size_t size() const {
    return size_;
  }

  const jchar& operator[](size_t n) const {
    return chars_[n];
  }

 private:
  JNIEnv* const env_;
  const jstring string_;
  const jchar* chars_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(ScopedStringChars);
};

#endif  // LAGDETECTOR_LAG_DETECTOR_MAIN_CPP_NATIVEHELPER_SCOPED_STRING_CHARS_H_