#ifndef __matrix_hprof_analyzer_internal_analyzer_h__
#define __matrix_hprof_analyzer_internal_analyzer_h__

#include "../include/matrix_hprof_analyzer.h"

#include <functional>
#include <string>
#include <optional>

#include "macro.h"

#include "heap.h"
#include "parser.h"

namespace matrix::hprof {

    class HprofAnalyzerImpl {
    public:
        static std::unique_ptr<HprofAnalyzerImpl> Create(int hprof_fd);

        HprofAnalyzerImpl(void *data, size_t data_size);

        ~HprofAnalyzerImpl();

        void ExcludeInstanceFieldReference(const std::string &class_name, const std::string &field_name);

        void ExcludeStaticFieldReference(const std::string &class_name, const std::string &field_name);

        void ExcludeThreadReference(const std::string &thread_name);

        void ExcludeNativeGlobalReference(const std::string &class_name);

        std::vector<LeakChain>
        Analyze(const std::function<std::vector<object_id_t>(const HprofHeap &)> &leak_finder);

        static std::vector<LeakChain>
        Analyze(const internal::heap::Heap &heap, const std::vector<object_id_t> &leaks);

        static std::optional<LeakChain>
        BuildLeakChain(const internal::heap::Heap &heap, const std::vector<std::pair<internal::heap::object_id_t,
                std::optional<internal::heap::reference_t>>> &chain);

        void *data_{};
        size_t data_size_{};
        const std::unique_ptr<const internal::parser::HeapParser> parser_;
        internal::parser::ExcludeMatcherGroup exclude_matcher_group_;
    };
}

#endif