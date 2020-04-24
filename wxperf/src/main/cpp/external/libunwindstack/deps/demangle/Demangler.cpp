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

#include <assert.h>

#include <cctype>
#include <stack>
#include <string>
#include <vector>

#include "Demangler.h"

constexpr const char* Demangler::kTypes[];
constexpr const char* Demangler::kDTypes[];
constexpr const char* Demangler::kSTypes[];

void Demangler::Save(const std::string& str, bool is_name) {
  saves_.push_back(str);
  last_save_name_ = is_name;
}

std::string Demangler::GetArgumentsString() {
  size_t num_args = cur_state_.args.size();
  std::string arg_str;
  if (num_args > 0) {
    arg_str = cur_state_.args[0];
    for (size_t i = 1; i < num_args; i++) {
      arg_str += ", " + cur_state_.args[i];
    }
  }
  return arg_str;
}

const char* Demangler::AppendOperatorString(const char* name) {
  const char* oper = nullptr;
  switch (*name) {
  case 'a':
    name++;
    switch (*name) {
    case 'a':
      oper = "operator&&";
      break;
    case 'd':
    case 'n':
      oper = "operator&";
      break;
    case 'N':
      oper = "operator&=";
      break;
    case 'S':
      oper = "operator=";
      break;
    }
    break;
  case 'c':
    name++;
    switch (*name) {
    case 'l':
      oper = "operator()";
      break;
    case 'm':
      oper = "operator,";
      break;
    case 'o':
      oper = "operator~";
      break;
    }
    break;
  case 'd':
    name++;
    switch (*name) {
    case 'a':
      oper = "operator delete[]";
      break;
    case 'e':
      oper = "operator*";
      break;
    case 'l':
      oper = "operator delete";
      break;
    case 'v':
      oper = "operator/";
      break;
    case 'V':
      oper = "operator/=";
      break;
    }
    break;
  case 'e':
    name++;
    switch (*name) {
    case 'o':
      oper = "operator^";
      break;
    case 'O':
      oper = "operator^=";
      break;
    case 'q':
      oper = "operator==";
      break;
    }
    break;
  case 'g':
    name++;
    switch (*name) {
    case 'e':
      oper = "operator>=";
      break;
    case 't':
      oper = "operator>";
      break;
    }
    break;
  case 'i':
    name++;
    switch (*name) {
    case 'x':
      oper = "operator[]";
      break;
    }
    break;
  case 'l':
    name++;
    switch (*name) {
    case 'e':
      oper = "operator<=";
      break;
    case 's':
      oper = "operator<<";
      break;
    case 'S':
      oper = "operator<<=";
      break;
    case 't':
      oper = "operator<";
      break;
    }
    break;
  case 'm':
    name++;
    switch (*name) {
    case 'i':
      oper = "operator-";
      break;
    case 'I':
      oper = "operator-=";
      break;
    case 'l':
      oper = "operator*";
      break;
    case 'L':
      oper = "operator*=";
      break;
    case 'm':
      oper = "operator--";
      break;
    }
    break;
  case 'n':
    name++;
    switch (*name) {
    case 'a':
      oper = "operator new[]";
      break;
    case 'e':
      oper = "operator!=";
      break;
    case 'g':
      oper = "operator-";
      break;
    case 't':
      oper = "operator!";
      break;
    case 'w':
      oper = "operator new";
      break;
    }
    break;
  case 'o':
    name++;
    switch (*name) {
    case 'o':
      oper = "operator||";
      break;
    case 'r':
      oper = "operator|";
      break;
    case 'R':
      oper = "operator|=";
      break;
    }
    break;
  case 'p':
    name++;
    switch (*name) {
    case 'm':
      oper = "operator->*";
      break;
    case 'l':
      oper = "operator+";
      break;
    case 'L':
      oper = "operator+=";
      break;
    case 'p':
      oper = "operator++";
      break;
    case 's':
      oper = "operator+";
      break;
    case 't':
      oper = "operator->";
      break;
    }
    break;
  case 'q':
    name++;
    switch (*name) {
    case 'u':
      oper = "operator?";
      break;
    }
    break;
  case 'r':
    name++;
    switch (*name) {
    case 'm':
      oper = "operator%";
      break;
    case 'M':
      oper = "operator%=";
      break;
    case 's':
      oper = "operator>>";
      break;
    case 'S':
      oper = "operator>>=";
      break;
    }
    break;
  }
  if (oper == nullptr) {
    return nullptr;
  }
  AppendCurrent(oper);
  cur_state_.last_save = oper;
  return name + 1;
}

const char* Demangler::GetStringFromLength(const char* name, std::string* str) {
  assert(std::isdigit(*name));

  size_t length = *name - '0';
  name++;
  while (*name != '\0' && std::isdigit(*name)) {
    length = length * 10 + *name - '0';
    name++;
  }

  std::string read_str;
  while (*name != '\0' && length != 0) {
    read_str += *name;
    name++;
    length--;
  }
  if (length != 0) {
    return nullptr;
  }
  // Special replacement of _GLOBAL__N_1 to (anonymous namespace).
  if (read_str == "_GLOBAL__N_1") {
    *str += "(anonymous namespace)";
  } else {
    *str += read_str;
  }
  return name;
}

void Demangler::AppendCurrent(const std::string& str) {
  if (!cur_state_.str.empty()) {
    cur_state_.str += "::";
  }
  cur_state_.str += str;
}

void Demangler::AppendCurrent(const char* str) {
  if (!cur_state_.str.empty()) {
    cur_state_.str += "::";
  }
  cur_state_.str += str;
}

const char* Demangler::ParseS(const char* name) {
  if (std::islower(*name)) {
    const char* type = kSTypes[*name - 'a'];
    if (type == nullptr) {
      return nullptr;
    }
    AppendCurrent(type);
    return name + 1;
  }

  if (saves_.empty()) {
    return nullptr;
  }

  if (*name == '_') {
    last_save_name_ = false;
    AppendCurrent(saves_[0]);
    return name + 1;
  }

  bool isdigit = std::isdigit(*name);
  if (!isdigit && !std::isupper(*name)) {
    return nullptr;
  }

  size_t index;
  if (isdigit) {
    index = *name - '0' + 1;
  } else {
    index = *name - 'A' + 11;
  }
  name++;
  if (*name != '_') {
    return nullptr;
  }

  if (index >= saves_.size()) {
    return nullptr;
  }

  last_save_name_ = false;
  AppendCurrent(saves_[index]);
  return name + 1;
}

const char* Demangler::ParseT(const char* name) {
  if (template_saves_.empty()) {
    return nullptr;
  }

  if (*name == '_') {
    last_save_name_ = false;
    AppendCurrent(template_saves_[0]);
    return name + 1;
  }

  // Need to get the total number.
  char* end;
  unsigned long int index = strtoul(name, &end, 10) + 1;
  if (name == end || *end != '_') {
    return nullptr;
  }

  if (index >= template_saves_.size()) {
    return nullptr;
  }

  last_save_name_ = false;
  AppendCurrent(template_saves_[index]);
  return end + 1;
}

const char* Demangler::ParseFunctionName(const char* name) {
  if (*name == 'E') {
    if (parse_funcs_.empty()) {
      return nullptr;
    }
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();

    // Remove the last saved part so that the full function name is not saved.
    // But only if the last save was not something like a substitution.
    if (!saves_.empty() && last_save_name_) {
      saves_.pop_back();
    }

    function_name_ += cur_state_.str;
    while (!cur_state_.suffixes.empty()) {
      function_suffix_ += cur_state_.suffixes.back();
      cur_state_.suffixes.pop_back();
    }
    cur_state_.Clear();

    return name + 1;
  }

  if (*name == 'I') {
    state_stack_.push(cur_state_);
    cur_state_.Clear();

    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseFunctionNameTemplate;
    return name + 1;
  }

  return ParseComplexString(name);
}

const char* Demangler::ParseFunctionNameTemplate(const char* name) {
  if (*name == 'E' && name[1] == 'E') {
    // Only consider this a template with saves if it is right before
    // the end of the name.
    template_found_ = true;
    template_saves_ = cur_state_.args;
  }
  return ParseTemplateArgumentsComplex(name);
}

const char* Demangler::ParseComplexArgument(const char* name) {
  if (*name == 'E') {
    if (parse_funcs_.empty()) {
      return nullptr;
    }
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();

    AppendArgument(cur_state_.str);
    cur_state_.str.clear();

    return name + 1;
  }

  return ParseComplexString(name);
}

void Demangler::FinalizeTemplate() {
  std::string arg_str(GetArgumentsString());
  cur_state_ = state_stack_.top();
  state_stack_.pop();
  cur_state_.str += '<' + arg_str + '>';
}

const char* Demangler::ParseComplexString(const char* name) {
  if (*name == 'S') {
    name++;
    if (*name == 't') {
      AppendCurrent("std");
      return name + 1;
    }
    return ParseS(name);
  }
  if (*name == 'L') {
    name++;
    if (!std::isdigit(*name)) {
      return nullptr;
    }
  }
  if (std::isdigit(*name)) {
    std::string str;
    name = GetStringFromLength(name, &str);
    if (name == nullptr) {
      return name;
    }
    AppendCurrent(str);
    Save(cur_state_.str, true);
    cur_state_.last_save = std::move(str);
    return name;
  }
  if (*name == 'D') {
    name++;
    if (saves_.empty() || (*name != '0' && *name != '1' && *name != '2'
        && *name != '5')) {
      return nullptr;
    }
    last_save_name_ = false;
    AppendCurrent("~" + cur_state_.last_save);
    return name + 1;
  }
  if (*name == 'C') {
    name++;
    if (saves_.empty() || (*name != '1' && *name != '2' && *name != '3'
        && *name != '5')) {
      return nullptr;
    }
    last_save_name_ = false;
    AppendCurrent(cur_state_.last_save);
    return name + 1;
  }
  if (*name == 'K') {
    cur_state_.suffixes.push_back(" const");
    return name + 1;
  }
  if (*name == 'V') {
    cur_state_.suffixes.push_back(" volatile");
    return name + 1;
  }
  if (*name == 'I') {
    // Save the current argument state.
    state_stack_.push(cur_state_);
    cur_state_.Clear();

    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseTemplateArgumentsComplex;
    return name + 1;
  }
  name = AppendOperatorString(name);
  if (name != nullptr) {
    Save(cur_state_.str, true);
  }
  return name;
}

void Demangler::AppendArgument(const std::string& str) {
  std::string arg(str);
  while (!cur_state_.suffixes.empty()) {
    arg += cur_state_.suffixes.back();
    cur_state_.suffixes.pop_back();
    Save(arg, false);
  }
  cur_state_.args.push_back(arg);
}

const char* Demangler::ParseFunctionArgument(const char* name) {
  if (*name == 'E') {
    // The first argument is the function modifier.
    // The second argument is the function type.
    // The third argument is the return type of the function.
    // The rest of the arguments are the function arguments.
    size_t num_args = cur_state_.args.size();
    if (num_args < 4) {
      return nullptr;
    }
    std::string function_modifier = cur_state_.args[0];
    std::string function_type = cur_state_.args[1];

    std::string str = cur_state_.args[2] + ' ';
    if (!cur_state_.args[1].empty()) {
      str += '(' + cur_state_.args[1] + ')';
    }

    if (num_args == 4 && cur_state_.args[3] == "void") {
      str += "()";
    } else {
      str += '(' + cur_state_.args[3];
      for (size_t i = 4; i < num_args; i++) {
        str += ", " + cur_state_.args[i];
      }
      str += ')';
    }
    str += cur_state_.args[0];

    cur_state_ = state_stack_.top();
    state_stack_.pop();
    cur_state_.args.emplace_back(std::move(str));

    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();
    return name + 1;
  }
  return ParseArguments(name);
}

const char* Demangler::ParseArguments(const char* name) {
  switch (*name) {
  case 'P':
    cur_state_.suffixes.push_back("*");
    return name + 1;

  case 'R':
    // This should always be okay because the string is guaranteed to have
    // at least two characters before this. A mangled string always starts
    // with _Z.
    if (name[-1] != 'R') {
      // Multiple 'R's in a row only add a single &.
      cur_state_.suffixes.push_back("&");
    }
    return name + 1;

  case 'O':
    cur_state_.suffixes.push_back("&&");
    return name + 1;

  case 'K':
  case 'V': {
    const char* suffix;
    if (*name == 'K') {
      suffix = " const";
    } else {
      suffix = " volatile";
    }
    if (!cur_state_.suffixes.empty() && (name[-1] == 'K' || name[-1] == 'V')) {
      // Special case, const/volatile apply as a single entity.
      size_t index = cur_state_.suffixes.size();
      cur_state_.suffixes[index-1].insert(0, suffix);
    } else {
      cur_state_.suffixes.push_back(suffix);
    }
    return name + 1;
  }

  case 'F': {
    std::string function_modifier;
    std::string function_type;
    if (!cur_state_.suffixes.empty()) {
      // If the first element starts with a ' ', then this modifies the
      // function itself.
      if (cur_state_.suffixes.back()[0] == ' ') {
        function_modifier = cur_state_.suffixes.back();
        cur_state_.suffixes.pop_back();
      }
      while (!cur_state_.suffixes.empty()) {
        function_type += cur_state_.suffixes.back();
        cur_state_.suffixes.pop_back();
      }
    }

    state_stack_.push(cur_state_);

    cur_state_.Clear();

    // The function parameter has this format:
    //   First argument is the function modifier.
    //   Second argument is the function type.
    //   Third argument will be the return function type but has not
    //     been parsed yet.
    //   Any other parameters are the arguments to the function. There
    //     must be at least one or this isn't valid.
    cur_state_.args.push_back(function_modifier);
    cur_state_.args.push_back(function_type);

    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseFunctionArgument;
    return name + 1;
  }

  case 'N':
    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseComplexArgument;
    return name + 1;

  case 'S':
    name++;
    if (*name == 't') {
      cur_state_.str = "std::";
      return name + 1;
    }
    name = ParseS(name);
    if (name == nullptr) {
      return nullptr;
    }
    AppendArgument(cur_state_.str);
    cur_state_.str.clear();
    return name;

  case 'D':
    name++;
    if (*name >= 'a' && *name <= 'z') {
      const char* arg = Demangler::kDTypes[*name - 'a'];
      if (arg == nullptr) {
        return nullptr;
      }
      AppendArgument(arg);
      return name + 1;
    }
    return nullptr;

  case 'I':
    // Save the current argument state.
    state_stack_.push(cur_state_);
    cur_state_.Clear();

    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseTemplateArguments;
    return name + 1;

  case 'v':
    AppendArgument("void");
    return name + 1;

  default:
    if (*name >= 'a' && *name <= 'z') {
      const char* arg = Demangler::kTypes[*name - 'a'];
      if (arg == nullptr) {
        return nullptr;
      }
      AppendArgument(arg);
      return name + 1;
    } else if (std::isdigit(*name)) {
      std::string arg = cur_state_.str;
      name = GetStringFromLength(name, &arg);
      if (name == nullptr) {
        return nullptr;
      }
      Save(arg, true);
      if (*name == 'I') {
        // There is one case where this argument is not complete, and that's
        // where this is a template argument.
        cur_state_.str = arg;
      } else {
        AppendArgument(arg);
        cur_state_.str.clear();
      }
      return name;
    } else if (strcmp(name, ".cfi") == 0) {
      function_suffix_ += " [clone .cfi]";
      return name + 4;
    }
  }
  return nullptr;
}

const char* Demangler::ParseTemplateLiteral(const char* name) {
  if (*name == 'E') {
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();
    return name + 1;
  }
  // Only understand boolean values with 0 or 1.
  if (*name == 'b') {
    name++;
    if (*name == '0') {
      AppendArgument("false");
      cur_state_.str.clear();
    } else if (*name == '1') {
      AppendArgument("true");
      cur_state_.str.clear();
    } else {
      return nullptr;
    }
    return name + 1;
  }
  return nullptr;
}

const char* Demangler::ParseTemplateArgumentsComplex(const char* name) {
  if (*name == 'E') {
    if (parse_funcs_.empty()) {
      return nullptr;
    }
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();

    FinalizeTemplate();
    Save(cur_state_.str, false);
    return name + 1;
  } else if (*name == 'L') {
    // Literal value for a template.
    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseTemplateLiteral;
    return name + 1;
  }

  return ParseArguments(name);
}

const char* Demangler::ParseTemplateArguments(const char* name) {
  if (*name == 'E') {
    if (parse_funcs_.empty()) {
      return nullptr;
    }
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();
    FinalizeTemplate();
    AppendArgument(cur_state_.str);
    cur_state_.str.clear();
    return name + 1;
  } else if (*name == 'L') {
    // Literal value for a template.
    parse_funcs_.push_back(parse_func_);
    parse_func_ = &Demangler::ParseTemplateLiteral;
    return name + 1;
  }

  return ParseArguments(name);
}

const char* Demangler::ParseFunctionTemplateArguments(const char* name) {
  if (*name == 'E') {
    parse_func_ = parse_funcs_.back();
    parse_funcs_.pop_back();

    function_name_ += '<' + GetArgumentsString() + '>';
    template_found_ = true;
    template_saves_ = cur_state_.args;
    cur_state_.Clear();
    return name + 1;
  }
  return ParseTemplateArgumentsComplex(name);
}

const char* Demangler::FindFunctionName(const char* name) {
  if (*name == 'T') {
    // non-virtual thunk, verify that it matches one of these patterns:
    //   Thn[0-9]+_
    //   Th[0-9]+_
    //   Thn_
    //   Th_
    name++;
    if (*name != 'h') {
      return nullptr;
    }
    name++;
    if (*name == 'n') {
      name++;
    }
    while (std::isdigit(*name)) {
      name++;
    }
    if (*name != '_') {
      return nullptr;
    }
    function_name_ = "non-virtual thunk to ";
    return name + 1;
  }

  if (*name == 'N') {
    parse_funcs_.push_back(&Demangler::ParseArgumentsAtTopLevel);
    parse_func_ = &Demangler::ParseFunctionName;
    return name + 1;
  }

  if (*name == 'S') {
    name++;
    if (*name == 't') {
      function_name_ = "std::";
      name++;
    } else {
      return nullptr;
    }
  }

  if (std::isdigit(*name)) {
    name = GetStringFromLength(name, &function_name_);
  } else if (*name == 'L' && std::isdigit(name[1])) {
    name = GetStringFromLength(name + 1, &function_name_);
  } else {
    name = AppendOperatorString(name);
    function_name_ = cur_state_.str;
  }
  cur_state_.Clear();

  // Check for a template argument, which will still be part of the function
  // name.
  if (name != nullptr && *name == 'I') {
    parse_funcs_.push_back(&Demangler::ParseArgumentsAtTopLevel);
    parse_func_ = &Demangler::ParseFunctionTemplateArguments;
    return name + 1;
  }
  parse_func_ = &Demangler::ParseArgumentsAtTopLevel;
  return name;
}

const char* Demangler::ParseArgumentsAtTopLevel(const char* name) {
  // At the top level is the only place where T is allowed.
  if (*name == 'T') {
    name++;
    name = ParseT(name);
    if (name == nullptr) {
      return nullptr;
    }
    AppendArgument(cur_state_.str);
    cur_state_.str.clear();
    return name;
  }

  return Demangler::ParseArguments(name);
}

std::string Demangler::Parse(const char* name, size_t max_length) {
  if (name[0] == '\0' || name[0] != '_' || name[1] == '\0' || name[1] != 'Z') {
    // Name is not mangled.
    return name;
  }

  Clear();

  parse_func_ = &Demangler::FindFunctionName;
  parse_funcs_.push_back(&Demangler::Fail);
  const char* cur_name = name + 2;
  while (cur_name != nullptr && *cur_name != '\0'
      && static_cast<size_t>(cur_name - name) < max_length) {
    cur_name = (this->*parse_func_)(cur_name);
  }
  if (cur_name == nullptr || *cur_name != '\0' || function_name_.empty() ||
      !cur_state_.suffixes.empty()) {
    return name;
  }

  std::string return_type;
  if (template_found_) {
    // Only a single argument with a template is not allowed.
    if (cur_state_.args.size() == 1) {
      return name;
    }

    // If there are at least two arguments, this template has a return type.
    if (cur_state_.args.size() > 1) {
      // The first argument will be the return value.
      return_type = cur_state_.args[0] + ' ';
      cur_state_.args.erase(cur_state_.args.begin());
    }
  }

  std::string arg_str;
  if (cur_state_.args.size() == 1 && cur_state_.args[0] == "void") {
    // If the only argument is void, then don't print any args.
    arg_str = "()";
  } else {
    arg_str = GetArgumentsString();
    if (!arg_str.empty()) {
      arg_str = '(' + arg_str + ')';
    }
  }
  return return_type + function_name_ + arg_str + function_suffix_;
}

std::string demangle(const char* name) {
  Demangler demangler;
  return demangler.Parse(name);
}
