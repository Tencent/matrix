#ifndef __matrix_resource_canary_memory_util_symbol_h__
#define __matrix_resource_canary_memory_util_symbol_h__

bool initialize_symbols();

void dump_heap(const char *file_name);

namespace mirror {
    class Thread {
    };
}

mirror::Thread *current_thread();

void suspend_runtime(mirror::Thread *thread);

void resume_runtime(mirror::Thread *thread);

#endif