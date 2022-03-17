#include "include/errorha.h"

#include <string>

thread_local static std::string error;

void set_matrix_hprof_analyzer_error(const std::string& message) {
  error = message;
}

const char* get_matrix_hprof_analyzer_error() {
  return error.c_str();
}

#ifdef __ANDROID__

#include <android/log.h>

#else

#include <stdexcept>

#endif

[[noreturn]] void fatal(const char *message) {
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_FATAL, "matrix_hprof_analyzer", "%s", message);
#else
    throw std::runtime_error(message);
#endif
}