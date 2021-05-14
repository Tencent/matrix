/*
 * Copyright (C) 2017 The Android Open Source Project
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

#ifndef __LIB_DEMANGLE_DEMANGLER_H
#define __LIB_DEMANGLE_DEMANGLER_H

#include <assert.h>

#include <stack>
#include <string>
#include <vector>

class Demangler {
 public:
  Demangler() = default;

  // NOTE: The max_length is not guaranteed to be the absolute max length
  // of a string that will be rejected. Under certain circumstances the
  // length check will not occur until after the second letter of a pair
  // is checked.
  std::string Parse(const char* name, size_t max_length = kMaxDefaultLength);

  void AppendCurrent(const std::string& str);
  void AppendCurrent(const char* str);
  void AppendArgument(const std::string& str);
  std::string GetArgumentsString();
  void FinalizeTemplate();
  const char* ParseS(const char* name);
  const char* ParseT(const char* name);
  const char* AppendOperatorString(const char* name);
  void Save(const std::string& str, bool is_name);

 private:
  void Clear() {
    parse_funcs_.clear();
    function_name_.clear();
    function_suffix_.clear();
    first_save_.clear();
    cur_state_.Clear();
    saves_.clear();
    template_saves_.clear();
    while (!state_stack_.empty()) {
      state_stack_.pop();
    }
    last_save_name_ = false;
    template_found_ = false;
  }

  using parse_func_type = const char* (Demangler::*)(const char*);
  parse_func_type parse_func_;
  std::vector<parse_func_type> parse_funcs_;
  std::vector<std::string> saves_;
  std::vector<std::string> template_saves_;
  bool last_save_name_;
  bool template_found_;

  std::string function_name_;
  std::string function_suffix_;

  struct StateData {
    void Clear() {
      str.clear();
      args.clear();
      prefix.clear();
      suffixes.clear();
      last_save.clear();
    }

    std::string str;
    std::vector<std::string> args;
    std::string prefix;
    std::vector<std::string> suffixes;
    std::string last_save;
  };
  std::stack<StateData> state_stack_;
  std::string first_save_;
  StateData cur_state_;

  static const char* GetStringFromLength(const char* name, std::string* str);

  // Parsing functions.
  const char* ParseComplexString(const char* name);
  const char* ParseComplexArgument(const char* name);
  const char* ParseArgumentsAtTopLevel(const char* name);
  const char* ParseArguments(const char* name);
  const char* ParseTemplateArguments(const char* name);
  const char* ParseTemplateArgumentsComplex(const char* name);
  const char* ParseTemplateLiteral(const char* name);
  const char* ParseFunctionArgument(const char* name);
  const char* ParseFunctionName(const char* name);
  const char* ParseFunctionNameTemplate(const char* name);
  const char* ParseFunctionTemplateArguments(const char* name);
  const char* FindFunctionName(const char* name);
  const char* Fail(const char*) { return nullptr; }

  // The default maximum string length string to process.
  static constexpr size_t kMaxDefaultLength = 2048;

  static constexpr const char* kTypes[] = {
    "signed char",        // a
    "bool",               // b
    "char",               // c
    "double",             // d
    "long double",        // e
    "float",              // f
    "__float128",         // g
    "unsigned char",      // h
    "int",                // i
    "unsigned int",       // j
    nullptr,              // k
    "long",               // l
    "unsigned long",      // m
    "__int128",           // n
    "unsigned __int128",  // o
    nullptr,              // p
    nullptr,              // q
    nullptr,              // r
    "short",              // s
    "unsigned short",     // t
    nullptr,              // u
    "void",               // v
    "wchar_t",            // w
    "long long",          // x
    "unsigned long long", // y
    "...",                // z
  };

  static constexpr const char* kDTypes[] = {
    "auto",               // a
    nullptr,              // b
    nullptr,              // c
    "decimal64",          // d
    "decimal128",         // e
    "decimal32",          // f
    nullptr,              // g
    "half",               // h
    "char32_t",           // i
    nullptr,              // j
    nullptr,              // k
    nullptr,              // l
    nullptr,              // m
    "decltype(nullptr)",  // n
    nullptr,              // o
    nullptr,              // p
    nullptr,              // q
    nullptr,              // r
    "char16_t",           // s
    nullptr,              // t
    nullptr,              // u
    nullptr,              // v
    nullptr,              // w
    nullptr,              // x
    nullptr,              // y
    nullptr,              // z
  };

  static constexpr const char* kSTypes[] = {
    "std::allocator",     // a
    "std::basic_string",  // b
    nullptr,              // c
    "std::iostream",      // d
    nullptr,              // e
    nullptr,              // f
    nullptr,              // g
    nullptr,              // h
    "std::istream",       // i
    nullptr,              // j
    nullptr,              // k
    nullptr,              // l
    nullptr,              // m
    nullptr,              // n
    "std::ostream",       // o
    nullptr,              // p
    nullptr,              // q
    nullptr,              // r
    "std::string",        // s
    nullptr,              // t
    nullptr,              // u
    nullptr,              // v
    nullptr,              // w
    nullptr,              // x
    nullptr,              // y
    nullptr,              // z
  };
};

#endif  // __LIB_DEMANGLE_DEMANGLER_H
