#include "parser.h"
#include "gtest/gtest.h"

#include "reader.h"
#include "mock/mock_engine.h"

using namespace test::mock;
using namespace testing;

namespace matrix::hprof::internal::parser {
    TEST(parser, parse) {
        HeapParser parser(new NiceMock<MockEngine>(sizeof(uint32_t)));
        auto *mock_engine = reinterpret_cast<NiceMock<MockEngine> *>(parser.engine_.get());
        EXPECT_CALL(*mock_engine, Parse(_, _, _, _));

        reader::Reader unused_reader(nullptr, 0);
        heap::Heap unused_heap;
        ExcludeMatcherGroup exclude_matcher_group;

        parser.Parse(unused_reader, unused_heap, exclude_matcher_group);
    }
}