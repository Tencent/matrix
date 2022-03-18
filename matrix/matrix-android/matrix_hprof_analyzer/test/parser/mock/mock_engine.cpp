#include "mock_engine.h"

using namespace matrix::hprof::internal;

namespace test::mock {

    MockEngineImpl::MockEngineImpl(size_t id_size) :
            parser::HeapParserEngine(),
            id_size_(id_size) {}

    bool MockEngineImpl::Parse(reader::Reader &reader, heap::Heap &heap,
                               const parser::ExcludeMatcherGroup &exclude_matchers,
                               const HeapParserEngine &next) const {
        return true;
    }

    bool MockEngineImpl::ParseHeader(reader::Reader &reader,
                                     heap::Heap &heap) const {
        reader.ReadNullTerminatedString();
        reader.Skip(sizeof(uint32_t) * 3);
        return true;
    }

    bool MockEngineImpl::ParseStringRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const {
        reader.Skip(record_length);
        return true;
    }

    bool MockEngineImpl::ParseLoadClassRecord(reader::Reader &reader, heap::Heap &heap, size_t record_length) const {
        reader.Skip(record_length);
        return true;
    }

    bool MockEngineImpl::ParseHeapContent(reader::Reader &reader, heap::Heap &heap, size_t record_length,
                                          const parser::ExcludeMatcherGroup &, const HeapParserEngine &) const {
        reader.Skip(record_length);
        return true;
    }

    bool MockEngineImpl::LazyParse(heap::Heap &heap, const parser::ExcludeMatcherGroup &exclude_matcher_group) const {
        return true;
    }

    size_t MockEngineImpl::ParseHeapContentRootUnknownSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootJniGlobalSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootJniLocalSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t) + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootJavaFrameSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t) + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootNativeStackSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootStickyClassSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootThreadBlockSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootMonitorUsedSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootThreadObjectSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t) + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootInternedStringSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootFinalizingSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootDebuggerSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t
    MockEngineImpl::ParseHeapContentRootReferenceCleanupSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootVMInternalSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootJniMonitorSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t) + sizeof(uint32_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentRootUnreachableSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::ParseHeapContentClassSubRecord(reader::Reader &reader, heap::Heap &heap,
                                                          const parser::ExcludeMatcherGroup &exclude_matcher_group) const {
        // class ID + stack trace serial number + super class ID + class loader object ID +
        // signers object ID + protection domain object ID + reserved * 2 + instance size
        const size_t skip_header_size = id_size_ + sizeof(uint32_t) + id_size_ * 6 + sizeof(uint32_t);
        reader.Skip(skip_header_size);

        // Skip constant pool.
        size_t skip_constant_pool_size = 0;
        {
            const uint16_t constant_pool_count = reader.ReadU2(); // constant pool elements count
            skip_constant_pool_size += sizeof(uint16_t);
            for (size_t i = 0; i < constant_pool_count; ++i) {
                reader.SkipU2();  // constant pool index
                skip_constant_pool_size += sizeof(uint16_t);
                const heap::value_type_t type = heap::value_type_cast(reader.ReadU1());  // constant type
                skip_constant_pool_size += sizeof(uint8_t);
                const size_t type_size =
                        type == heap::value_type_t::kObject ? id_size_ : heap::get_value_type_size(type);
                reader.Skip(type_size);  // constant value
                skip_constant_pool_size += type_size;
            }
        }

        // Skip static fields.
        size_t skip_static_fields_size = 0;
        {
            const uint16_t static_fields_count = reader.ReadU2();  // static fields count
            skip_static_fields_size += sizeof(uint16_t);
            for (size_t i = 0; i < static_fields_count; ++i) {
                reader.Skip(id_size_);  // static field name ID
                skip_static_fields_size += id_size_;
                const heap::value_type_t type = heap::value_type_cast(reader.ReadU1());  // static field type
                skip_static_fields_size += sizeof(uint8_t);
                const size_t type_size =
                        type == heap::value_type_t::kObject ? id_size_ : heap::get_value_type_size(type);
                reader.Skip(type_size);  // static field value
                skip_static_fields_size += type_size;
            }
        }

        // Skip instance fields.
        const uint16_t instance_fields_count = reader.ReadU2();  // instance fields count
        reader.Skip(instance_fields_count * (id_size_ + sizeof(uint8_t)));  // instance fields elements
        const size_t skip_instance_fields_size =
                sizeof(uint16_t) + instance_fields_count * (id_size_ + sizeof(uint8_t));

        return skip_header_size + skip_constant_pool_size + skip_static_fields_size + skip_instance_fields_size;
    }

    size_t MockEngineImpl::ParseHeapContentInstanceSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        reader.Skip(id_size_);  // object ID
        reader.SkipU4();  // stack trace serial number
        reader.Skip(id_size_);  // class ID
        const size_t fields_data_size = reader.ReadU4();  // fields data size
        reader.Skip(fields_data_size);  // fields data content
        return id_size_ + sizeof(uint32_t) + id_size_ + sizeof(uint32_t) + fields_data_size;
    }

    size_t MockEngineImpl::ParseHeapContentObjectArraySubRecord(reader::Reader &reader, heap::Heap &heap) const {
        reader.Skip(id_size_);  // array object ID
        reader.SkipU4();  // stack trace serial number
        const size_t array_length = reader.ReadU4();  // array length
        reader.Skip(id_size_);  // array element class ID
        reader.Skip(array_length * id_size_);  // array content
        return id_size_ + sizeof(uint32_t) + sizeof(uint32_t) + id_size_ + array_length * id_size_;
    }

    size_t MockEngineImpl::ParseHeapContentPrimitiveArraySubRecord(reader::Reader &reader, heap::Heap &heap) const {
        reader.Skip(id_size_);  // array object ID
        reader.SkipU4();  // stack trace serial number
        const size_t array_length = reader.ReadU4();  // array length
        const heap::value_type_t type = heap::value_type_cast(reader.ReadU1());  // array type
        const size_t type_size = heap::get_value_type_size(type);
        reader.Skip(array_length * type_size);  // array content
        return id_size_ + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) + array_length * type_size;
    }

    size_t
    MockEngineImpl::ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader::Reader &reader, heap::Heap &heap) const {
        const size_t skip_size = id_size_ + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
        reader.Skip(skip_size);
        return skip_size;
    }

    size_t MockEngineImpl::SkipHeapContentInfoSubRecord(reader::Reader &reader, const heap::Heap &heap) const {
        const size_t skip_size = sizeof(uint32_t) + id_size_;
        reader.Skip(skip_size);
        return skip_size;
    }
}