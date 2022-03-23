#include "engine.h"
#include "gtest/gtest.h"

#include "buffer_generator.h"

#include "mock/mock_engine.h"

#include "errorha.h"

#include <bit>

using namespace matrix::hprof::internal::heap;
using namespace matrix::hprof::internal::parser;
using namespace matrix::hprof::internal::reader;

using namespace test::tools;
using namespace test::mock;

using namespace testing;

typedef size_t identifier_t;

static constexpr size_t kMaxRecordSize = (1 << 5) - 1;
static constexpr size_t kMaxItemCount = 0xf;

static const ExcludeMatcherGroup empty_exclude_matcher_group;

TEST(parser_engine, main) {
    HeapParserEngineImpl engine;
    Heap heap_unused;

    NiceMock<MockEngine> mock_engine(sizeof(identifier_t));

    const size_t string_records_count = 5;
    const size_t heap_content_records_count = 5;
    const std::string buffer = ({
        BufferGenerator generator;
        // Header.
        {
            generator.WriteNullTerminatedString("Version String");
            generator.Write<uint32_t>(sizeof(identifier_t));
            generator.WriteZero(sizeof(uint64_t));
        }
        // Strings.
        {
            for (size_t i = 0; i < string_records_count; ++i) {
                generator.Write<uint8_t>(0x01);
                generator.WriteZero(sizeof(uint32_t));
                const size_t content_size = kMaxRecordSize;
                generator.Write<uint32_t>(content_size);
                generator.WriteZero(content_size);
            }
        }
        // Load classes.
        {
            generator.Write<uint8_t>(0x02);
            generator.WriteZero(sizeof(uint32_t));
            const size_t content_size = kMaxRecordSize;
            generator.Write<uint32_t>(content_size);
            generator.WriteZero(content_size);
        }
        // Heap content.
        {
            for (size_t i = 0; i < heap_content_records_count; ++i) {
                const uint8_t tag = (i & 1) == 0 ? 0x0c : 0x1c;
                generator.Write<uint8_t>(tag);
                generator.WriteZero(sizeof(uint32_t));
                const size_t content_size = kMaxRecordSize;
                generator.Write<uint32_t>(content_size);
                generator.WriteZero(content_size);
            }
        }
        // Others.
        {
            generator.Write<uint8_t>(0xff);
            generator.WriteZero(sizeof(uint32_t));
            const size_t content_size = kMaxRecordSize;
            generator.Write<uint32_t>(content_size);
            generator.WriteZero(content_size);
        }
        // End.
        {
            generator.Write<uint8_t>(0x2c);
            generator.WriteZero(sizeof(uint32_t));
            generator.Write<uint32_t>(0);
        }
        generator.GetContent();
    });

    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

    EXPECT_CALL(mock_engine, ParseHeader(_, _));
    EXPECT_CALL(mock_engine, ParseStringRecord(_, _, _))
            .Times(string_records_count);
    EXPECT_CALL(mock_engine, ParseLoadClassRecord(_, _, _));
    EXPECT_CALL(mock_engine, ParseHeapContent(_, _, _, _, _))
            .Times(heap_content_records_count);
    EXPECT_CALL(mock_engine, LazyParse(_, _));

    engine.Parse(reader, heap_unused, empty_exclude_matcher_group, mock_engine);
}

TEST(parser_engine, header) {
    HeapParserEngineImpl engine;
    {
        const std::string buffer = ({
            BufferGenerator generator;
            generator.WriteNullTerminatedString("JAVA PROFILE 1.0");
            generator.Write<uint32_t>(3);
            generator.WriteZero(sizeof(uint64_t));
            generator.GetContent();
        });
        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
        Heap heap;
        EXPECT_NO_THROW(engine.ParseHeader(reader, heap));
        EXPECT_EQ(heap.GetIdSize(), 3);
    }
    {
        const std::string buffer = ({
            BufferGenerator generator;
            generator.WriteNullTerminatedString("JAVA PROFILE 1.0.1");
            generator.Write<uint32_t>(4);
            generator.WriteZero(sizeof(uint64_t));
            generator.GetContent();
        });
        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
        Heap heap;
        EXPECT_NO_THROW(engine.ParseHeader(reader, heap));
        EXPECT_EQ(heap.GetIdSize(), 4);
    }
    {
        const std::string buffer = ({
            BufferGenerator generator;
            generator.WriteNullTerminatedString("JAVA PROFILE 1.0.2");
            generator.Write<uint32_t>(5);
            generator.WriteZero(sizeof(uint64_t));
            generator.GetContent();
        });
        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
        Heap heap;
        EXPECT_NO_THROW(engine.ParseHeader(reader, heap));
        EXPECT_EQ(heap.GetIdSize(), 5);
    }
    {
        const std::string buffer = ({
            BufferGenerator generator;
            generator.WriteNullTerminatedString("JAVA PROFILE 1.0.3");
            generator.Write<uint32_t>(7);
            generator.WriteZero(sizeof(uint64_t));
            generator.GetContent();
        });
        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
        Heap heap;
        EXPECT_NO_THROW(engine.ParseHeader(reader, heap));
        EXPECT_EQ(heap.GetIdSize(), 7);
    }
    {
        const std::string buffer = ({
            BufferGenerator generator;
            generator.WriteNullTerminatedString("JAVA PROFILE 1.0.0");
            generator.Write<uint32_t>(2);
            generator.WriteZero(sizeof(uint64_t));
            generator.GetContent();
        });
        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
        Heap heap;
        EXPECT_THROW(engine.ParseHeader(reader, heap), std::runtime_error);
    }
}

TEST(parser_engine, strings) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x101);
        generator.WriteString("Hello world!");
        generator.GetContent();
    });

    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    engine.ParseStringRecord(reader, heap, buffer.size());
    EXPECT_EQ("Hello world!", heap.GetString(0x101));
}

TEST(parser_engine, load_class) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<identifier_t>(0x101);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    engine.ParseLoadClassRecord(reader, heap, buffer.size());
    EXPECT_EQ(0x101, heap.GetClassNameId(0x001));
}

TEST(parser_engine, heap_content) {
    const std::string buffer = ({
        BufferGenerator generator;
        // root unknown
        generator.Write<uint8_t>(0xff);
        generator.WriteZero(sizeof(identifier_t));
        // root Jni global
        generator.Write<uint8_t>(0x01);
        generator.WriteZero(sizeof(identifier_t) + sizeof(identifier_t));
        // root Jni local
        generator.Write<uint8_t>(0x02);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        // root Java frame
        generator.Write<uint8_t>(0x03);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        // root native stack
        generator.Write<uint8_t>(0x04);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t));
        // root sticky class
        generator.Write<uint8_t>(0x05);
        generator.WriteZero(sizeof(identifier_t));
        // root thread block
        generator.Write<uint8_t>(0x06);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t));
        // root monitor used
        generator.Write<uint8_t>(0x07);
        generator.WriteZero(sizeof(identifier_t));
        // root thread object
        generator.Write<uint8_t>(0x08);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        // root interned string
        generator.Write<uint8_t>(0x89);
        generator.WriteZero(sizeof(identifier_t));
        // root finalizing
        generator.Write<uint8_t>(0x8a);
        generator.WriteZero(sizeof(identifier_t));
        // root debugger
        generator.Write<uint8_t>(0x8b);
        generator.WriteZero(sizeof(identifier_t));
        // root reference cleanup
        generator.Write<uint8_t>(0x8c);
        generator.WriteZero(sizeof(identifier_t));
        // root VM internal
        generator.Write<uint8_t>(0x8d);
        generator.WriteZero(sizeof(identifier_t));
        // root Jni monitor
        generator.Write<uint8_t>(0x8e);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        // root unreachable
        generator.Write<uint8_t>(0x90);
        generator.WriteZero(sizeof(identifier_t));
        // class
        generator.Write<uint8_t>(0x20);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(identifier_t) * 6
                            + sizeof(uint32_t) + sizeof(uint16_t) * 3);
        // instance
        generator.Write<uint8_t>(0x21);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(identifier_t) + sizeof(uint32_t));
        // object array
        generator.Write<uint8_t>(0x22);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(identifier_t));
        // primitive array
        generator.Write<uint8_t>(0x23);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        generator.Write<uint8_t>(static_cast<uint8_t>(value_type_t::kByte));
        // primitive array no data
        generator.Write<uint8_t>(0xc3);
        generator.WriteZero(sizeof(identifier_t) + sizeof(uint32_t) + sizeof(uint32_t));
        generator.Write<uint8_t>(static_cast<uint8_t>(value_type_t::kByte));
        // heap dump info
        generator.Write<uint8_t>(0xfe);
        generator.WriteZero(sizeof(uint32_t) + sizeof(identifier_t));

        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));

    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

    HeapParserEngineImpl engine;
    MockEngine mock_engine(sizeof(identifier_t));

    EXPECT_CALL(mock_engine, ParseHeapContentRootUnknownSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootJniGlobalSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootJniLocalSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootJavaFrameSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootNativeStackSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootStickyClassSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootThreadBlockSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootMonitorUsedSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootThreadObjectSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootInternedStringSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootFinalizingSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootDebuggerSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootReferenceCleanupSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootVMInternalSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootJniMonitorSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentRootUnreachableSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentClassSubRecord(_, _, _));
    EXPECT_CALL(mock_engine, ParseHeapContentInstanceSubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentObjectArraySubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentPrimitiveArraySubRecord(_, _));
    EXPECT_CALL(mock_engine, ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(_, _));
    EXPECT_CALL(mock_engine, SkipHeapContentInfoSubRecord(_, _));

    engine.ParseHeapContent(reader, heap, buffer.size(), empty_exclude_matcher_group, mock_engine);
}

TEST(parser_engine, lazy_parse_instances) {
    // Prepare heap.
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    { // classes
        heap.AddString(0x111, "test.A");
        heap.AddClassNameRecord(0x011, 0x111);

        heap.AddString(0x112, "test.B");
        heap.AddClassNameRecord(0x012, 0x112);

        heap.AddString(0x113, "test.C");
        heap.AddClassNameRecord(0x013, 0x113);

        heap.AddString(0x114, "test.ExcludeByField");
        heap.AddClassNameRecord(0x014, 0x114);

        heap.AddInheritanceRecord(0x012, 0x011);
        heap.AddInheritanceRecord(0x013, 0x012);
        heap.AddInheritanceRecord(0x014, 0x013);
    }
    { // fields
        heap.AddString(0x101, "instanceInclude");
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id = 0x101, .type = value_type_t::kObject});

        heap.AddString(0x102, "instanceExclude");
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id = 0x102, .type = value_type_t::kObject});

        heap.AddString(0x103, "instanceExcludeByClass");
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id = 0x103, .type = value_type_t::kObject});

        heap.AddString(0x104, "primitive");
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id = 0x104, .type = value_type_t::kByte});
    }
    // instance
    const std::string instance_fields_data = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x005);
        generator.Write<identifier_t>(0x006);
        generator.Write<identifier_t>(0x007);
        generator.Write<uint8_t>(0x1);
        generator.GetContent();
    });
    {
        heap.AddInstanceTypeRecord(0x001, object_type_t::kInstance);
        heap.AddInstanceClassRecord(0x001, 0x011);
        Reader fields_data_reader(reinterpret_cast<const uint8_t *>(instance_fields_data.data()),
                                  instance_fields_data.size());
        heap.ReadFieldsData(0x001, 0x011, instance_fields_data.size(), &fields_data_reader);
    }
    {
        heap.AddInstanceTypeRecord(0x002, object_type_t::kInstance);
        heap.AddInstanceClassRecord(0x002, 0x012);
        Reader fields_data_reader(reinterpret_cast<const uint8_t *>(instance_fields_data.data()),
                                  instance_fields_data.size());
        heap.ReadFieldsData(0x002, 0x012, instance_fields_data.size(), &fields_data_reader);
    }
    {
        heap.AddInstanceTypeRecord(0x003, object_type_t::kInstance);
        heap.AddInstanceClassRecord(0x003, 0x013);
        Reader fields_data_reader(reinterpret_cast<const uint8_t *>(instance_fields_data.data()),
                                  instance_fields_data.size());
        heap.ReadFieldsData(0x003, 0x013, instance_fields_data.size(), &fields_data_reader);
    }
    {
        heap.AddInstanceTypeRecord(0x004, object_type_t::kInstance);
        heap.AddInstanceClassRecord(0x004, 0x014);
        Reader fields_data_reader(reinterpret_cast<const uint8_t *>(instance_fields_data.data()),
                                  instance_fields_data.size());
        heap.ReadFieldsData(0x004, 0x014, instance_fields_data.size(), &fields_data_reader);
    }
    HeapParserEngineImpl engine;
    ExcludeMatcherGroup exclude_matcher_group;
    {
        exclude_matcher_group.instance_fields_.emplace_back("test.B", "instanceExclude");
        exclude_matcher_group.instance_fields_.emplace_back("test.ExcludeByField", "*");
        exclude_matcher_group.instance_fields_.emplace_back("*", "instanceExcludeByClass");
    }
    engine.LazyParse(heap, exclude_matcher_group);
    { // primitives
        {
            const auto *data = heap.ScopedGetPrimitiveData(0x001, 0x104);
            EXPECT_EQ(value_type_t::kByte, data->GetType());
            EXPECT_EQ(0x1, data->GetValue<uint8_t>());
        }
        {
            const auto *data = heap.ScopedGetPrimitiveData(0x002, 0x104);
            EXPECT_EQ(value_type_t::kByte, data->GetType());
            EXPECT_EQ(0x1, data->GetValue<uint8_t>());
        }
        {
            const auto *data = heap.ScopedGetPrimitiveData(0x003, 0x104);
            EXPECT_EQ(value_type_t::kByte, data->GetType());
            EXPECT_EQ(0x1, data->GetValue<uint8_t>());
        }
        {
            const auto *data = heap.ScopedGetPrimitiveData(0x004, 0x104);
            EXPECT_EQ(value_type_t::kByte, data->GetType());
            EXPECT_EQ(0x1, data->GetValue<uint8_t>());
        }
    }
    { // exclude
        {
            EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x001).at(0x005).field_name_id);
            EXPECT_EQ(reference_type_t::kInstanceField, heap.GetLeakReferenceGraph().at(0x001).at(0x005).type);
            EXPECT_EQ(0x005, heap.GetFieldReference(0x001, "instanceInclude"));

            EXPECT_EQ(0x102, heap.GetLeakReferenceGraph().at(0x001).at(0x006).field_name_id);
            EXPECT_EQ(reference_type_t::kInstanceField, heap.GetLeakReferenceGraph().at(0x001).at(0x006).type);
            EXPECT_EQ(0x006, heap.GetFieldReference(0x001, "instanceExclude"));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x001).at(0x007), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x001, "instanceExcludeByClass"));
            EXPECT_EQ(0x007, heap.GetFieldReference(0x001, "instanceExcludeByClass", true));
        }
        {
            EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x002).at(0x005).field_name_id);
            EXPECT_EQ(reference_type_t::kInstanceField, heap.GetLeakReferenceGraph().at(0x002).at(0x005).type);
            EXPECT_EQ(0x005, heap.GetFieldReference(0x002, "instanceInclude"));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x002).at(0x006), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x002, "instanceExclude"));
            EXPECT_EQ(0x006, heap.GetFieldReference(0x002, "instanceExclude", true));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x002).at(0x007), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x002, "instanceExcludeByClass"));
            EXPECT_EQ(0x007, heap.GetFieldReference(0x002, "instanceExcludeByClass", true));
        }
        {
            EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x003).at(0x005).field_name_id);
            EXPECT_EQ(reference_type_t::kInstanceField, heap.GetLeakReferenceGraph().at(0x003).at(0x005).type);
            EXPECT_EQ(0x005, heap.GetFieldReference(0x003, "instanceInclude"));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x003).at(0x006), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x003, "instanceExclude"));
            EXPECT_EQ(0x006, heap.GetFieldReference(0x003, "instanceExclude", true));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x003).at(0x007), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x003, "instanceExcludeByClass"));
            EXPECT_EQ(0x007, heap.GetFieldReference(0x003, "instanceExcludeByClass", true));
        }
        {
            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x004).at(0x005), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x004, "instanceInclude"));
            EXPECT_EQ(0x005, heap.GetFieldReference(0x004, "instanceInclude", true));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x004).at(0x006), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x004, "instanceExclude"));
            EXPECT_EQ(0x006, heap.GetFieldReference(0x004, "instanceExclude", true));

            EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x004).at(0x007), std::out_of_range);
            EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x004, "instanceExcludeByClass"));
            EXPECT_EQ(0x007, heap.GetFieldReference(0x004, "instanceExcludeByClass", true));
        }
    }
}

TEST(parser_engine, lazy_parse_exclude_thread) {
    Heap heap;
    { // strings
        const uint8_t include_buffer[] = {
                // string: "include"
                0x69, 0x6e, 0x63, 0x6c, 0x75, 0x64, 0x65
        };
        const uint8_t exclude_buffer[] = {
                // string: "exclude"
                0x65, 0x78, 0x63, 0x6c, 0x75, 0x64, 0x65
        };

        heap.AddString(0x111, "java.lang.String");
        heap.AddString(0x101, "value");
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id = 0x101, .type = value_type_t::kObject});

        // string: "include"
        Reader include_reader(include_buffer, sizeof(include_buffer));
        heap.AddInstanceClassRecord(0x001, 0x011);
        heap.ReadPrimitiveArray(0x002, value_type_t::kByte, sizeof(include_buffer), &include_reader);
        heap.AddFieldReference(0x001, 0x101, 0x002);
        // string: "exclude"
        Reader exclude_reader(exclude_buffer, sizeof(exclude_buffer));
        heap.AddInstanceClassRecord(0x003, 0x011);
        heap.ReadPrimitiveArray(0x004, value_type_t::kByte, sizeof(exclude_buffer), &exclude_reader);
        heap.AddFieldReference(0x003, 0x101, 0x004);
    }
    { // threads
        heap.AddString(0x112, "java.lang.Thread");
        heap.AddString(0x102, "name");
        heap.AddClassNameRecord(0x012, 0x112);
        heap.AddInstanceFieldRecord(0x012, field_t{.name_id = 0x102, .type = value_type_t::kObject});

        // thread: "include"
        heap.AddInstanceClassRecord(0x005, 0x012);
        heap.AddFieldReference(0x005, 0x102, 0x001);
        heap.AddThreadObjectRecord(0x005, 0x201);
        // thread: "exclude"
        heap.AddInstanceClassRecord(0x006, 0x012);
        heap.AddFieldReference(0x006, 0x102, 0x003);
        heap.AddThreadObjectRecord(0x006, 0x202);
    }
    { // GC roots
        heap.AddThreadReferenceRecord(0x007, 0x201);
        heap.MarkGcRoot(0x007, heap::gc_root_type_t::kRootJavaFrame);
        heap.AddThreadReferenceRecord(0x008, 0x202);
        heap.MarkGcRoot(0x008, heap::gc_root_type_t::kRootJavaFrame);
    }
    { // leaks
        heap.AddFieldReference(0x007, 0x103, 0x009);
        heap.AddFieldReference(0x008, 0x103, 0x00a);
    }
    ExcludeMatcherGroup exclude_matcher_group;
    exclude_matcher_group.threads_.emplace_back("exclude");

    HeapParserEngineImpl engine;
    engine.LazyParse(heap, exclude_matcher_group);

    EXPECT_EQ(0x103, heap.GetLeakReferenceGraph().at(0x007).at(0x009).field_name_id);
    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x008).at(0x00a), std::out_of_range);
}

TEST(parser_engine, lazy_parse_exclude_native_global) {
    Heap heap;
    { // strings and classes
        heap.AddString(0x111, "test.ExcludeClass");
        heap.AddInstanceTypeRecord(0x011, object_type_t::kClass);
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x112, "test.ExcludeArrayElement[]");
        heap.AddInstanceTypeRecord(0x012, object_type_t::kClass);
        heap.AddClassNameRecord(0x012, 0x112);
    }
    { // instances
        heap.AddInstanceClassRecord(0x001, 0x011);
        heap.MarkGcRoot(0x001, heap::gc_root_type_t::kRootJniGlobal);
        heap.AddInstanceClassRecord(0x002, 0x012);
        heap.MarkGcRoot(0x002, heap::gc_root_type_t::kRootJniGlobal);
    }
    heap.AddFieldReference(0x001, 0x101, 0x003);
    heap.AddArrayReference(0x002, 0, 0x004);

    ExcludeMatcherGroup exclude_matcher_group;
    exclude_matcher_group.native_globals_.emplace_back("test.ExcludeClass");
    exclude_matcher_group.native_globals_.emplace_back("test.ExcludeArrayElement[]");

    HeapParserEngineImpl engine;
    engine.LazyParse(heap, exclude_matcher_group);

    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x001).at(0x003), std::out_of_range);
    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x002).at(0x004), std::out_of_range);
}

TEST(parser_engine, heap_content_error_handle) {
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<uint8_t>(0x00);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    HeapParserEngineImpl engine;
    MockEngine mock_engine(sizeof(identifier_t));
    EXPECT_THROW(engine.ParseHeapContent(
            reader, heap, buffer.size(),
            empty_exclude_matcher_group, mock_engine
    ), std::runtime_error);
}

TEST(parser_engine, root_unknown) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootUnknownSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootUnknown, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_jni_global) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(identifier_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootJniGlobalSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootJniGlobal, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_jni_local) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t) + sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootJniLocalSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootJniLocal, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_java_frame) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.Write<uint32_t>(0x201);
        generator.WriteZero(sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootJavaFrameSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootJavaFrame, heap.GetGcRootType(0x001));
    EXPECT_EQ(0x201, heap.GetThreadReference(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_native_stack) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootNativeStackSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootNativeStack, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_sticky_class) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootStickyClassSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootStickyClass, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_thread_block) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootThreadBlockSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootThreadBlock, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_monitor_used) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootMonitorUsedSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootMonitorUsed, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_thread_object) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.Write<uint32_t>(0x201);
        generator.WriteZero(sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootThreadObjectSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootThreadObject, heap.GetGcRootType(0x001));
    EXPECT_EQ(0x001, heap.GetThreadObject(0x201));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_interned_string) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootInternedStringSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootInternedString, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_finalizing) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootFinalizingSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootFinalizing, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_debugger) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootDebuggerSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootDebugger, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_reference_cleanup) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootReferenceCleanupSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootReferenceCleanup, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_vm_internal) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootVMInternalSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootVMInternal, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_jni_monitor) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t) + sizeof(uint32_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootJniMonitorSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootJniMonitor, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, root_unreachable) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentRootUnreachableSubRecord(reader, heap);
    const std::vector<heap::object_id_t> gc_roots = heap.GetGcRoots();
    EXPECT_TRUE(std::find(gc_roots.begin(), gc_roots.end(), 0x001) != gc_roots.end());
    EXPECT_EQ(heap::gc_root_type_t::kRootUnreachable, heap.GetGcRootType(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, record_class) {
    HeapParserEngineImpl engine;
    const size_t constant_pool_count = kMaxItemCount;
    const size_t static_fields_object_count = 2;
    const size_t static_fields_primitive_count = kMaxItemCount;
    const size_t instance_fields_count = kMaxItemCount;

    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x011);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<identifier_t>(0x012);
        generator.WriteZero(sizeof(identifier_t) * 5);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<uint16_t>(constant_pool_count);
        for (size_t i = 0; i < constant_pool_count; ++i) {
            generator.Write<uint16_t>(i);
            generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kByte));
            generator.Write<uint8_t>(i);
        }
        generator.Write<uint16_t>(static_fields_object_count + static_fields_primitive_count);
        for (size_t i = 0; i < static_fields_object_count; ++i) {
            generator.Write<identifier_t>(0x101 + i);
            generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kObject));
            generator.Write<identifier_t>(0x003 + i);
        }
        for (size_t i = 0; i < static_fields_primitive_count; ++i) {
            generator.Write<identifier_t>(0x120 + i);
            generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kInt));
            generator.Write<uint32_t>(i);
        }
        generator.Write<uint16_t>(instance_fields_count);
        for (size_t i = 0; i < instance_fields_count; ++i) {
            generator.Write<identifier_t>(0x120 + i);
            generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kByte));
        }
        generator.GetContent();
    });

    { // default
        Heap heap;
        heap.InitializeIdSize(sizeof(identifier_t));
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x111, "test.SampleClass");
        heap.AddString(0x101, "staticA");
        heap.AddString(0x102, "staticB");

        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

        const size_t parse_size = engine.ParseHeapContentClassSubRecord(reader, heap, empty_exclude_matcher_group);

        EXPECT_EQ(heap::object_type_t::kClass, heap.GetInstanceType(0x011));
        EXPECT_EQ(0x012, heap.GetSuperClass(0x011));

        EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x011).at(003).field_name_id);
        EXPECT_EQ(reference_type_t::kStaticField, heap.GetLeakReferenceGraph().at(0x011).at(003).type);
        EXPECT_EQ(0x003, heap.GetFieldReference(0x011, "staticA"));

        EXPECT_EQ(0x102, heap.GetLeakReferenceGraph().at(0x011).at(0x004).field_name_id);
        EXPECT_EQ(reference_type_t::kStaticField, heap.GetLeakReferenceGraph().at(0x011).at(0x004).type);
        EXPECT_EQ(0x004, heap.GetFieldReference(0x011, "staticB"));

        std::vector<field_t> instance_fields = heap.GetInstanceFields(0x011);
        std::sort(instance_fields.begin(), instance_fields.end(), [](const field_t &a, const field_t &b) {
            return a.name_id < b.name_id;
        });
        for (size_t i = 0; i < instance_fields_count; ++i) {
            EXPECT_EQ(0x120 + i, instance_fields.at(i).name_id);
        }
    }
    { // exclude current
        Heap heap;
        heap.InitializeIdSize(sizeof(identifier_t));
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x111, "test.SampleClass");
        heap.AddString(0x101, "staticInclude");
        heap.AddString(0x102, "staticExclude");

        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

        ExcludeMatcherGroup exclude_matcher_group;
        exclude_matcher_group.static_fields_.emplace_back("test.SampleClass", "staticExclude");

        const size_t parse_size = engine.ParseHeapContentClassSubRecord(reader, heap, exclude_matcher_group);

        EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x011).at(003).field_name_id);
        EXPECT_EQ(reference_type_t::kStaticField, heap.GetLeakReferenceGraph().at(0x011).at(003).type);
        EXPECT_EQ(0x003, heap.GetFieldReference(0x011, "staticInclude"));

        EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x011).at(0x004), std::out_of_range);
        EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x011, "staticExclude"));
        EXPECT_EQ(0x004, heap.GetFieldReference(0x011, "staticExclude", true));
    }
    { // exclude super
        Heap heap;
        heap.InitializeIdSize(sizeof(identifier_t));
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x111, "test.SampleClass");
        heap.AddClassNameRecord(0x012, 0x112);
        heap.AddString(0x112, "test.SuperClass");
        heap.AddString(0x101, "staticInclude");
        heap.AddString(0x102, "staticExclude");

        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

        ExcludeMatcherGroup exclude_matcher_group;
        exclude_matcher_group.static_fields_.emplace_back("test.SuperClass", "staticExclude");

        const size_t parse_size = engine.ParseHeapContentClassSubRecord(reader, heap, exclude_matcher_group);

        EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x011).at(003).field_name_id);
        EXPECT_EQ(reference_type_t::kStaticField, heap.GetLeakReferenceGraph().at(0x011).at(003).type);
        EXPECT_EQ(0x003, heap.GetFieldReference(0x011, "staticInclude"));

        EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x011).at(0x004), std::out_of_range);
        EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x011, "staticExclude"));
        EXPECT_EQ(0x004, heap.GetFieldReference(0x011, "staticExclude", true));
    }
    { // exclude all classes
        Heap heap;
        heap.InitializeIdSize(sizeof(identifier_t));
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x111, "test.SampleClass");
        heap.AddString(0x101, "staticInclude");
        heap.AddString(0x102, "staticExclude");

        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

        ExcludeMatcherGroup exclude_matcher_group;
        exclude_matcher_group.static_fields_.emplace_back("*", "staticExclude");

        const size_t parse_size = engine.ParseHeapContentClassSubRecord(reader, heap, exclude_matcher_group);

        EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x011).at(003).field_name_id);
        EXPECT_EQ(reference_type_t::kStaticField, heap.GetLeakReferenceGraph().at(0x011).at(003).type);
        EXPECT_EQ(0x003, heap.GetFieldReference(0x011, "staticInclude"));

        EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x011).at(0x004), std::out_of_range);
        EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x011, "staticExclude"));
        EXPECT_EQ(0x004, heap.GetFieldReference(0x011, "staticExclude", true));
    }
    { // exclude all fields
        Heap heap;
        heap.InitializeIdSize(sizeof(identifier_t));
        heap.AddClassNameRecord(0x011, 0x111);
        heap.AddString(0x111, "test.SampleClass");
        heap.AddString(0x101, "staticInclude");
        heap.AddString(0x102, "staticExclude");

        Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());

        ExcludeMatcherGroup exclude_matcher_group;
        exclude_matcher_group.static_fields_.emplace_back("test.SampleClass", "*");

        const size_t parse_size = engine.ParseHeapContentClassSubRecord(reader, heap, exclude_matcher_group);

        EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x011).at(003), std::out_of_range);
        EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x011, "staticInclude"));
        EXPECT_EQ(0x003, heap.GetFieldReference(0x011, "staticInclude", true));

        EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x011).at(0x004), std::out_of_range);
        EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x011, "staticExclude"));
        EXPECT_EQ(0x004, heap.GetFieldReference(0x011, "staticExclude", true));
    }
}

TEST(parser_engine, record_instance) {
    HeapParserEngineImpl engine;
    const size_t fields_data_size = kMaxRecordSize;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<identifier_t>(0x002);
        generator.Write<uint32_t>(fields_data_size);
        generator.WriteZero(fields_data_size);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentInstanceSubRecord(reader, heap);
    EXPECT_EQ(heap::object_type_t::kInstance, heap.GetInstanceType(0x001));
    EXPECT_EQ(0x002, heap.GetClass(0x001));
    const heap::HeapFieldsData *fields_data = heap.ScopedGetFieldsData(0x001);
    EXPECT_NE(nullptr, fields_data);
    EXPECT_EQ(0x001, fields_data->GetInstanceId());
    EXPECT_EQ(0x002, fields_data->GetClassId());
    EXPECT_EQ(fields_data_size, fields_data->GetSize());
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, record_object_array) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<uint32_t>(2);
        generator.Write<identifier_t>(0x011);
        generator.Write<identifier_t>(0x002);
        generator.Write<identifier_t>(0x003);
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentObjectArraySubRecord(reader, heap);
    EXPECT_EQ(heap::object_type_t::kObjectArray, heap.GetInstanceType(0x001));
    EXPECT_EQ(0x011, heap.GetClass(0x001));
    EXPECT_EQ(0, heap.GetLeakReferenceGraph().at(0x001).at(0x002).index);
    EXPECT_EQ(reference_type_t::kArrayElement, heap.GetLeakReferenceGraph().at(0x001).at(0x002).type);
    EXPECT_EQ(1, heap.GetLeakReferenceGraph().at(0x001).at(0x003).index);
    EXPECT_EQ(reference_type_t::kArrayElement, heap.GetLeakReferenceGraph().at(0x001).at(0x003).type);
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, record_primitive_array) {
    HeapParserEngineImpl engine;
    const size_t array_length = 10;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<uint32_t>(array_length);
        generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kInt));
        generator.WriteZero(array_length * get_value_type_size(heap::value_type_t::kInt));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentPrimitiveArraySubRecord(reader, heap);
    EXPECT_EQ(heap::object_type_t::kPrimitiveArray, heap.GetInstanceType(0x001));
    const HeapPrimitiveArrayData *array_data = heap.ScopedGetPrimitiveArrayData(0x001);
    EXPECT_NE(nullptr, array_data);
    EXPECT_EQ(heap::value_type_t::kInt, array_data->GetType());
    EXPECT_EQ(array_length, array_data->GetLength());
    EXPECT_EQ(array_length * get_value_type_size(heap::value_type_t::kInt), array_data->GetSize());
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, record_primitive_array_no_data) {
    HeapParserEngineImpl engine;
    const size_t array_length = 10;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.Write<identifier_t>(0x001);
        generator.WriteZero(sizeof(uint32_t));
        generator.Write<uint32_t>(array_length);
        generator.Write<uint8_t>(static_cast<uint8_t>(heap::value_type_t::kFloat));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    const size_t parse_size = engine.ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader, heap);
    EXPECT_EQ(heap::object_type_t::kPrimitiveArray, heap.GetInstanceType(0x001));
    EXPECT_EQ(nullptr, heap.ScopedGetPrimitiveArrayData(0x001));
    EXPECT_EQ(buffer.size(), parse_size);
}

TEST(parser_engine, record_heap_info) {
    HeapParserEngineImpl engine;
    const std::string buffer = ({
        BufferGenerator generator;
        generator.WriteZero(sizeof(uint32_t) + sizeof(identifier_t));
        generator.GetContent();
    });
    Heap heap;
    heap.InitializeIdSize(sizeof(identifier_t));
    Reader reader(reinterpret_cast<const uint8_t *>(buffer.data()), buffer.size());
    EXPECT_EQ(buffer.size(), engine.SkipHeapContentInfoSubRecord(reader, heap));
}