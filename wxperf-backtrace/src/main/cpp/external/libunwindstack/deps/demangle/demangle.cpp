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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cctype>
#include <string>

#include <demangle.h>

extern "C" char* __cxa_demangle(const char*, char*, size_t*, int*);

static void Usage(const char* prog_name) {
  printf("usage: %s [-c] [NAME_TO_DEMANGLE...]\n", prog_name);
  printf("\n");
  printf("Demangles C++ mangled names if supplied on the command-line, or found\n");
  printf("reading from stdin otherwise.\n");
  printf("\n");
  printf("-c\tCompare against __cxa_demangle\n");
  printf("\n");
}

static std::string DemangleWithCxa(const char* name) {
  const char* cxa_demangle = __cxa_demangle(name, nullptr, nullptr, nullptr);
  if (cxa_demangle == nullptr) {
    return name;
  }

  // The format of our demangler is slightly different from the cxa demangler
  // so modify the cxa demangler output. Specifically, for templates, remove
  // the spaces between '>' and '>'.
  std::string demangled_str;
  for (size_t i = 0; i < strlen(cxa_demangle); i++) {
    if (i > 2 && cxa_demangle[i] == '>' && std::isspace(cxa_demangle[i - 1]) &&
        cxa_demangle[i - 2] == '>') {
      demangled_str.resize(demangled_str.size() - 1);
    }
    demangled_str += cxa_demangle[i];
  }
  return demangled_str;
}

static void Compare(const char* name, const std::string& demangled_name) {
  std::string cxa_demangled_name(DemangleWithCxa(name));
  if (cxa_demangled_name != demangled_name) {
    printf("\nMismatch!\n");
    printf("\tmangled name: %s\n", name);
    printf("\tour demangle: %s\n", demangled_name.c_str());
    printf("\tcxa demangle: %s\n", cxa_demangled_name.c_str());
    exit(1);
  }
}

static int Filter(bool compare) {
  char* line = nullptr;
  size_t line_length = 0;

  while ((getline(&line, &line_length, stdin)) != -1) {
    char* p = line;
    char* name;
    while ((name = strstr(p, "_Z")) != nullptr) {
      // Output anything before the identifier.
      *name = 0;
      printf("%s", p);
      *name = '_';

      // Extract the identifier.
      p = name;
      while (*p && (std::isalnum(*p) || *p == '_' || *p == '.' || *p == '$')) ++p;

      // Demangle and output.
      std::string identifier(name, p);
      std::string demangled_name = demangle(identifier.c_str());
      printf("%s", demangled_name.c_str());

      if (compare) Compare(identifier.c_str(), demangled_name);
    }
    // Output anything after the last identifier.
    printf("%s", p);
  }

  free(line);
  return 0;
}

int main(int argc, char** argv) {
#ifdef __BIONIC__
  const char* prog_name = getprogname();
#else
  const char* prog_name = argv[0];
#endif

  bool compare = false;
  int opt_char;
  while ((opt_char = getopt(argc, argv, "c")) != -1) {
    if (opt_char == 'c') {
      compare = true;
    } else {
      Usage(prog_name);
      return 1;
    }
  }

  // With no arguments, act as a filter.
  if (optind == argc) return Filter(compare);

  // Otherwise demangle each argument.
  while (optind < argc) {
    const char* name = argv[optind++];
    std::string demangled_name = demangle(name);
    printf("%s\n", demangled_name.c_str());

    if (compare) Compare(name, demangled_name);
  }
  return 0;
}
