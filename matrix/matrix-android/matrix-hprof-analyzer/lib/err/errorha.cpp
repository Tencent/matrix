#include "include/errorha.h"

#include <unistd.h>

static error_listener_t error_listener = nullptr;

error_listener_t set_matrix_hprof_analyzer_error_listener(error_listener_t listener) {
    const error_listener_t previous = error_listener;
    error_listener = listener;
    return previous;
}

void pub_error(const std::string& message) {
    if (error_listener != nullptr) {
        error_listener(message.c_str());
    }
}

[[noreturn]] void pub_fatal(const std::string& message) {
    pub_error(message);
#if __test_mode__
    throw std::runtime_error(message);
#else
    _exit(10);
#endif
}