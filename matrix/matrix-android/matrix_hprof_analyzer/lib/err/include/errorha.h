#ifndef __matrix_hprof_analyzer_err_h__
#define __matrix_hprof_analyzer_err_h__

#include <string>

typedef void (*error_listener_t)(const char *message);

error_listener_t set_matrix_hprof_analyzer_error_listener(error_listener_t listener);

void pub_error(const std::string& message);

[[noreturn]] void pub_fatal(const std::string& message);

#endif