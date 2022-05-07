#include "include/parser.h"
#include "internal/engine.h"

namespace matrix::hprof::internal::parser {

    HeapParser::HeapParser() :
            engine_(new HeapParserEngineImpl()) {}

    HeapParser::HeapParser(HeapParserEngine *engine) :
            engine_(engine) {}

    HeapParser::~HeapParser() = default;

    void
    HeapParser::Parse(reader::Reader &reader, heap::Heap &heap,
                      const ExcludeMatcherGroup &exclude_matcher_group) const {
        engine_->Parse(reader, heap, exclude_matcher_group, *engine_);
    }
}
