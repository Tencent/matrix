#ifndef __matrix_hprof_analyzer_parser_h__
#define __matrix_hprof_analyzer_parser_h__

#include <optional>
#include <utility>
#include <unistd.h>

#include "heap.h"

#include "macro.h"

namespace matrix::hprof::internal::parser {

    class FieldExcludeMatcher {
    public:
        FieldExcludeMatcher(std::string class_name, std::string field_name) :
                class_name_full_match_(class_name == "*"), field_name_full_match_(field_name == "*"),
                class_name_(class_name == "*" ? "" : std::move(class_name)),
                field_name_(field_name == "*" ? "" : std::move(field_name)) {}

        [[nodiscard]] bool FullMatchClassName() const {
            return class_name_full_match_;
        }

        [[nodiscard]] const std::string &GetClassName() const {
            return class_name_;
        }

        [[nodiscard]] bool FullMatchFieldName() const {
            return field_name_full_match_;
        }

        [[nodiscard]] const std::string &GetFieldName() const {
            return field_name_;
        }

    private:
        const bool class_name_full_match_;
        const std::string class_name_;
        const bool field_name_full_match_;
        const std::string field_name_;
    };

    class ThreadExcludeMatcher {
    public:
        explicit ThreadExcludeMatcher(std::string thread_name) :
                thread_name_full_match_(thread_name == "*"),
                thread_name_(thread_name == "*" ? "" : std::move(thread_name)) {}

        [[nodiscard]] bool FullMatchThreadName() const {
            return thread_name_full_match_;
        }

        [[nodiscard]] const std::string &GetThreadName() const {
            return thread_name_;
        }

    private:
        const bool thread_name_full_match_;
        const std::string thread_name_;
    };

    class NativeGlobalExcludeMatcher {
    public:
        explicit NativeGlobalExcludeMatcher(std::string class_name) :
                class_name_full_match_(class_name == "*"),
                class_name_(class_name == "*" ? "" : std::move(class_name)) {}

        [[nodiscard]] bool FullMatchClassName() const {
            return class_name_full_match_;
        }

        [[nodiscard]] const std::string &GetClassName() const {
            return class_name_;
        }

    private:
        const bool class_name_full_match_;
        const std::string class_name_;
    };

    class ExcludeMatcherGroup {
    public:
        std::vector<FieldExcludeMatcher> instance_fields_;
        std::vector<FieldExcludeMatcher> static_fields_;
        std::vector<ThreadExcludeMatcher> threads_;
        std::vector<NativeGlobalExcludeMatcher> native_globals_;
    };

    class HeapParserEngine;

    class HeapParser {
    public:
        HeapParser();

    private:
        friend_test(parser, parse);

        explicit HeapParser(HeapParserEngine *engine);

    public:
        ~HeapParser();

        void Parse(reader::Reader &reader, heap::Heap &heap, const ExcludeMatcherGroup &exclude_matcher_group) const;

    private:
        const std::unique_ptr<HeapParserEngine> engine_;
    };
}

#endif