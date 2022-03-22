#ifndef __matrix_hprof_analyzer_err_h__
#define __matrix_hprof_analyzer_err_h__

#include <string>

void set_matrix_hprof_analyzer_error(const std::string& message);

const char* get_matrix_hprof_analyzer_error();

[[noreturn]] void fatal(const char* message);

#endif