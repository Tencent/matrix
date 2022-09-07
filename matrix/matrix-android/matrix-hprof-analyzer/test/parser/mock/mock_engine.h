#ifndef __matrix_hprof_analyzer_parser_test_mock_engine_h__
#define __matrix_hprof_analyzer_parser_test_mock_engine_h__

#include "gmock/gmock.h"

#include "engine.h"

using namespace matrix::hprof::internal;

namespace test::mock {

    class MockEngineImpl final : public parser::HeapParserEngine {
    public:
        explicit MockEngineImpl(size_t id_size);

        void Parse(reader::Reader &reader, heap::Heap &heap, const parser::ExcludeMatcherGroup &exclude_matchers,
                   const HeapParserEngine &next) const override;

        void ParseHeader(reader::Reader &reader, heap::Heap &heap) const override;

        void ParseStringRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const override;

        void ParseLoadClassRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const override;

        void ParseHeapContent(reader::Reader &reader, heap::Heap &heap, size_t record_length,
                              const parser::ExcludeMatcherGroup &exclude_matchers,
                              const HeapParserEngine &next) const override;

        void LazyParse(heap::Heap &heap, const parser::ExcludeMatcherGroup &exclude_matcher_group) const override;

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
                                              const parser::ExcludeMatcherGroup &exclude_matcher_group) const override;

        size_t ParseHeapContentInstanceSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentObjectArraySubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t ParseHeapContentPrimitiveArraySubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t
        ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader::Reader &reader, heap::Heap &heap) const override;

        size_t SkipHeapContentInfoSubRecord(reader::Reader &reader, const heap::Heap &heap) const override;

    private:
        size_t id_size_;
    };

    class MockEngine : public parser::HeapParserEngine {
    public:
        explicit MockEngine(size_t id_size) : impl_(id_size) {
            ON_CALL(*this, Parse)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap,
                                   const parser::ExcludeMatcherGroup &exclude_matchers,
                                   const HeapParserEngine &next) {
                                impl_.Parse(reader, heap, exclude_matchers, next);
                            }
                    );

            ON_CALL(*this, ParseHeader)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                impl_.ParseHeader(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseStringRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap, size_t record_length) {
                                impl_.ParseStringRecord(reader, heap, record_length);
                            }
                    );

            ON_CALL(*this, ParseLoadClassRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap, size_t record_length) {
                                impl_.ParseLoadClassRecord(reader, heap, record_length);
                            }
                    );

            ON_CALL(*this, ParseHeapContent)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap, size_t record_length,
                                   const parser::ExcludeMatcherGroup &exclude_matchers, const HeapParserEngine &next) {
                                impl_.ParseHeapContent(reader, heap, record_length, exclude_matchers, next);
                            }
                    );

            ON_CALL(*this, LazyParse)
                    .WillByDefault(
                            [this](heap::Heap &heap, const parser::ExcludeMatcherGroup &exclude_matchers) {
                                impl_.LazyParse(heap, exclude_matchers);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootUnknownSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootUnknownSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootJniGlobalSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootJniGlobalSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootJniLocalSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootJniLocalSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootJavaFrameSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootJavaFrameSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootNativeStackSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootNativeStackSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootStickyClassSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootStickyClassSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootThreadBlockSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootThreadBlockSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootMonitorUsedSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootMonitorUsedSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootThreadObjectSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootThreadObjectSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootInternedStringSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootInternedStringSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootFinalizingSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootFinalizingSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootDebuggerSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootDebuggerSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootReferenceCleanupSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootReferenceCleanupSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootVMInternalSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootVMInternalSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootJniMonitorSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootJniMonitorSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentRootUnreachableSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentRootUnreachableSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentClassSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap,
                                   const parser::ExcludeMatcherGroup &exclude_matcher_group) {
                                return impl_.ParseHeapContentClassSubRecord(reader, heap, exclude_matcher_group);
                            }
                    );

            ON_CALL(*this, ParseHeapContentInstanceSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentInstanceSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentObjectArraySubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentObjectArraySubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentPrimitiveArraySubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentPrimitiveArraySubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, ParseHeapContentPrimitiveArrayNoDataDumpSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, heap::Heap &heap) {
                                return impl_.ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader, heap);
                            }
                    );

            ON_CALL(*this, SkipHeapContentInfoSubRecord)
                    .WillByDefault(
                            [this](reader::Reader &reader, const heap::Heap &heap) {
                                return impl_.SkipHeapContentInfoSubRecord(reader, heap);
                            }
                    );
        }

        MOCK_METHOD(void, Parse,
                    ((reader::Reader & reader), (heap::Heap & heap),
                            (const parser::ExcludeMatcherGroup &exclude_matcher_group), (const HeapParserEngine &next)),
                    (const, override));

        MOCK_METHOD(void, ParseHeader, ((reader::Reader & reader), (heap::Heap & heap)), (const, override));

        MOCK_METHOD(void, ParseStringRecord, ((reader::Reader & reader), (heap::Heap & heap), (size_t)),
                    (const, override));

        MOCK_METHOD(void, ParseLoadClassRecord,
                    ((reader::Reader & reader), (heap::Heap & heap), (size_t)), (const, override));

        MOCK_METHOD(void, ParseHeapContent,
                    ((reader::Reader & reader), (heap::Heap & heap), (size_t),
                            (const parser::ExcludeMatcherGroup &exclude_matcher_group), (const HeapParserEngine &next)),
                    (const, override));

        MOCK_METHOD(void, LazyParse, ((heap::Heap & heap),(const parser::ExcludeMatcherGroup &exclude_matcher_group)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootUnknownSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootJniGlobalSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootJniLocalSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootJavaFrameSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootNativeStackSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootStickyClassSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootThreadBlockSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootMonitorUsedSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootThreadObjectSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootInternedStringSubRecord,
                    ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootFinalizingSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootDebuggerSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootReferenceCleanupSubRecord,
                    ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootVMInternalSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootJniMonitorSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentRootUnreachableSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentClassSubRecord,
                    ((reader::Reader & reader), (heap::Heap & heap),
                            (const parser::ExcludeMatcherGroup &exclude_matcher_group)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentInstanceSubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentObjectArraySubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentPrimitiveArraySubRecord, ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, ParseHeapContentPrimitiveArrayNoDataDumpSubRecord,
                    ((reader::Reader & reader), (heap::Heap & heap)),
                    (const, override));

        MOCK_METHOD(size_t, SkipHeapContentInfoSubRecord, ((reader::Reader & reader),(const heap::Heap&)),
                    (const, override));

    private:
        MockEngineImpl impl_;
    };
}

#endif