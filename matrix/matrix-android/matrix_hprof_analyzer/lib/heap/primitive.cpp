#include "errorha.h"
#include "include/heap.h"

namespace matrix::hprof::internal::heap {
    HeapPrimitiveData::HeapPrimitiveData(value_type_t type, reader::Reader *source_reader) :
            type_(({
                if (type == value_type_t::kObject) fatal("Type is not primitive.");
                type;
            })),
            data_(({
                uint64_t data = 0;
                switch (get_value_type_size(type)) {
                    case sizeof(uint8_t):
                        *reinterpret_cast<uint8_t *>(&data) = source_reader->ReadU1();
                        break;
                    case sizeof(uint16_t):
                        *reinterpret_cast<uint16_t *>(&data) = source_reader->ReadU2();
                        break;
                    case sizeof(uint32_t):
                        *reinterpret_cast<uint32_t *>(&data) = source_reader->ReadU4();
                        break;
                    case sizeof(uint64_t):
                        *reinterpret_cast<uint64_t *>(&data) = source_reader->ReadU8();
                        break;
                }
                data;
            })) {}

    HeapPrimitiveArrayData::HeapPrimitiveArrayData(value_type_t type, size_t data_size,
                                                   reader::Reader *source_reader) :
            type_(({
                if (type == value_type_t::kObject) fatal("Type is not primitive.");
                type;
            })),
            data_size_(data_size),
            data_(reinterpret_cast<const uint8_t *>(source_reader->Extract(data_size))) {}
}