#ifndef __matrix_hprof_analyzer_parser_engine_h__
#define __matrix_hprof_analyzer_parser_engine_h__

#include "../include/parser.h"

#include "reader.h"

namespace matrix::hprof::internal::parser {

    /**
     * The only production implementation of the engine interface must implements functions pure for testability.
     * <p>
     * There are several functions have parameter "next". The "next" engine is used to parse sub-level HPROF record. In
     * the only implementation, it is always be set to the engine instance itself, but it will be replaced by a mock
     * engine while testing the implementation.
     */
    class HeapParserEngine {
    public:
        virtual ~HeapParserEngine() = default;

        virtual void Parse(reader::Reader &reader, heap::Heap &heap, const ExcludeMatcherGroup &exclude_matchers,
                           const HeapParserEngine &next) const = 0;

        virtual void ParseHeader(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual void ParseStringRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const = 0;

        virtual void ParseLoadClassRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const = 0;

        virtual void ParseHeapContent(reader::Reader &reader, heap::Heap &heap, size_t record_length,
                                      const ExcludeMatcherGroup &exclude_matchers,
                                      const HeapParserEngine &next) const = 0;

        virtual void LazyParse(heap::Heap &heap, const ExcludeMatcherGroup &exclude_matcher_group) const = 0;

        virtual size_t
        ParseHeapContentRootUnknownSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootJniGlobalSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootJniLocalSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootJavaFrameSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootNativeStackSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootStickyClassSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootThreadBlockSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootMonitorUsedSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootThreadObjectSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootInternedStringSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootFinalizingSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootDebuggerSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootReferenceCleanupSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootVMInternalSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootJniMonitorSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentRootUnreachableSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentClassSubRecord(reader::Reader &reader, heap::Heap &heap,
                                       const ExcludeMatcherGroup &exclude_matcher_group) const = 0;

        virtual size_t
        ParseHeapContentInstanceSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentObjectArraySubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentPrimitiveArraySubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader::Reader &reader, heap::Heap &heap) const = 0;

        virtual size_t
        SkipHeapContentInfoSubRecord(reader::Reader &reader, const heap::Heap &heap) const = 0;
    };

    /**
     * Parser engine implementation, see HeapParserEngine.
     */
    class HeapParserEngineImpl final : public HeapParserEngine {
    public:
        void Parse(reader::Reader &reader, heap::Heap &heap, const ExcludeMatcherGroup &exclude_matcher_group,
                   const HeapParserEngine &next) const override;

        void ParseHeader(reader::Reader &reader, heap::Heap &heap) const override;

        void ParseStringRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const override;

        void ParseLoadClassRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const override;

        void ParseHeapContent(reader::Reader &reader, heap::Heap &heap, size_t record_length,
                              const ExcludeMatcherGroup &exclude_matcher_group,
                              const HeapParserEngine &next) const override;

        void LazyParse(heap::Heap &heap, const ExcludeMatcherGroup &exclude_matcher_group) const override;

        size_t ParseHeapContentRootUnknownSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootJniGlobalSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootJniLocalSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootJavaFrameSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootNativeStackSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootStickyClassSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootThreadBlockSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootMonitorUsedSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootThreadObjectSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootInternedStringSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootFinalizingSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootDebuggerSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootReferenceCleanupSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootVMInternalSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootJniMonitorSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentRootUnreachableSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentClassSubRecord(reader::Reader &reader, heap::Heap &heap,
                                              const ExcludeMatcherGroup &exclude_matcher_group) const override;

        size_t ParseHeapContentInstanceSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentObjectArraySubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentPrimitiveArraySubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t
        ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t
        SkipHeapContentInfoSubRecord(reader::Reader &reader, const heap::Heap &heap) const override;
    };
}

#endif