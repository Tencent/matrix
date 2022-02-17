#include "heap.h"
#include "gtest/gtest.h"

using namespace matrix::hprof::internal::heap;
using namespace matrix::hprof::internal::reader;

TEST(heap_value_type, get_size) {
    EXPECT_EQ(0, get_value_type_size(value_type_t::kObject));
    EXPECT_EQ(1, get_value_type_size(value_type_t::kBoolean));
    EXPECT_EQ(2, get_value_type_size(value_type_t::kChar));
    EXPECT_EQ(4, get_value_type_size(value_type_t::kFloat));
    EXPECT_EQ(8, get_value_type_size(value_type_t::kDouble));
    EXPECT_EQ(1, get_value_type_size(value_type_t::kByte));
    EXPECT_EQ(2, get_value_type_size(value_type_t::kShort));
    EXPECT_EQ(4, get_value_type_size(value_type_t::kInt));
    EXPECT_EQ(8, get_value_type_size(value_type_t::kLong));
}

TEST(heap_value_type, cast) {
    EXPECT_EQ(value_type_t::kObject, value_type_cast(2));
    EXPECT_EQ(value_type_t::kBoolean, value_type_cast(4));
    EXPECT_EQ(value_type_t::kChar, value_type_cast(5));
    EXPECT_EQ(value_type_t::kFloat, value_type_cast(6));
    EXPECT_EQ(value_type_t::kDouble, value_type_cast(7));
    EXPECT_EQ(value_type_t::kByte, value_type_cast(8));
    EXPECT_EQ(value_type_t::kShort, value_type_cast(9));
    EXPECT_EQ(value_type_t::kInt, value_type_cast(10));
    EXPECT_EQ(value_type_t::kLong, value_type_cast(11));
    EXPECT_THROW(value_type_cast(0), std::runtime_error);
}

TEST(heap_fields_data, construct) {
    const uint8_t buffer[] = {0x00, 0x00, 0x00, 0x01, 0x02};
    Reader source_reader(buffer, sizeof(buffer));
    HeapFieldsData data(0x001, 0x011, 4, &source_reader);
    EXPECT_EQ(0x001, data.GetInstanceId());
    EXPECT_EQ(0x011, data.GetClassId());
    EXPECT_EQ(4, data.GetSize());
    EXPECT_EQ(1, data.GetScopeReader().ReadU4());
    EXPECT_EQ(2, source_reader.ReadU1());
}

TEST(heap_fields_data, scope_reader_independence) {
    const uint8_t buffer[] = {0x01, 0x02};
    Reader source_reader(buffer, sizeof(buffer));
    HeapFieldsData data(0x001, 0x011, 2, &source_reader);
    Reader scope_reader_1 = data.GetScopeReader();
    Reader scope_reader_2 = data.GetScopeReader();
    EXPECT_EQ(0x01, scope_reader_1.ReadU1());
    EXPECT_EQ(0x01, scope_reader_2.ReadU1());
    EXPECT_EQ(0x02, scope_reader_1.ReadU1());
    EXPECT_EQ(0x02, scope_reader_2.ReadU1());
}

TEST(heap, id_size) {
    Heap heap;
    EXPECT_THROW(auto i = heap.GetIdSize(), std::runtime_error);
    EXPECT_THROW(heap.InitializeIdSize(0), std::runtime_error);
    heap.InitializeIdSize(3);
    EXPECT_EQ(3, heap.GetIdSize());
    EXPECT_THROW(heap.InitializeIdSize(2), std::runtime_error);
}

TEST(heap, class_name) {
    Heap heap;
    heap.AddString(0x101, "test.SampleClass");
    heap.AddClassNameRecord(0x001, 0x101);
    EXPECT_EQ(0x101, heap.GetClassNameId(0x001));
    EXPECT_EQ("test.SampleClass", heap.GetClassName(0x001));
    EXPECT_THROW(auto i = heap.GetClassNameId(0x002), std::runtime_error);
    EXPECT_EQ(std::nullopt, heap.GetClassName(0x002));
    EXPECT_EQ(0x001, heap.FindClassByName("test.SampleClass"));
    EXPECT_EQ(0x001, heap.FindClassByName("test.SampleClass")); // Test memorized.
    EXPECT_EQ(std::nullopt, heap.FindClassByName("test.UnknownClass"));
    EXPECT_EQ(std::nullopt, heap.FindClassByName("test.UnknownClass")); // Test memorized.
}

TEST(heap, inheritance) {
    Heap heap;
    heap.AddInheritanceRecord(0x001, 0x002);
    heap.AddInheritanceRecord(0x002, 0x003);
    EXPECT_EQ(0x002, heap.GetSuperClass(0x001));
    EXPECT_EQ(0x003, heap.GetSuperClass(0x002));
    EXPECT_EQ(std::nullopt, heap.GetSuperClass(0x003));
    EXPECT_TRUE(heap.ChildClassOf(0x001, 0x001));
    EXPECT_TRUE(heap.ChildClassOf(0x001, 0x003));
    EXPECT_FALSE(heap.ChildClassOf(0x001, 0x004));
}

TEST(heap, instance_field) {
    Heap heap;
    const std::vector<field_t> expected = {
            field_t{.name_id=0x101, .type=value_type_t::kInt},
            field_t{.name_id=0x102, .type=value_type_t::kObject}
    };

    heap.AddInstanceFieldRecord(0x001, expected[0]);
    heap.AddInstanceFieldRecord(0x001, expected[1]);

    std::vector<field_t> actual = heap.GetInstanceFields(0x001);
    EXPECT_EQ(2, actual.size());
    std::sort(actual.begin(), actual.end(), [](const field_t &a, const field_t &b) {
        return a.name_id < b.name_id;
    });
    EXPECT_EQ(expected, actual);

    EXPECT_EQ(std::vector<field_t>(), heap.GetInstanceFields(0x002));
}

TEST(heap, instance_type) {
    Heap heap;
    // Upgrade.
    heap.AddInstanceTypeRecord(0x001, object_type_t::kInstance);
    EXPECT_EQ(object_type_t::kInstance, heap.GetInstanceType(0x001));
    // Exception thrown if absent.
    EXPECT_THROW(auto i = heap.GetInstanceType(0x002), std::runtime_error);
}

TEST(heap, instance_class) {
    Heap heap;
    heap.AddInheritanceRecord(0x011, 0x012);
    heap.AddInstanceClassRecord(0x001, 0x011);
    heap.AddInstanceClassRecord(0x002, 0x011);
    EXPECT_EQ(0x011, heap.GetClass(0x001));
    EXPECT_EQ(0x011, heap.GetClass(0x002));
    EXPECT_EQ(std::nullopt, heap.GetClass(0x003));
    std::vector<object_id_t> actual = heap.GetInstances(0x011);
    std::sort(actual.begin(), actual.end());
    EXPECT_EQ(0x001, actual[0]);
    EXPECT_EQ(0x002, actual[1]);
    EXPECT_EQ(std::vector<object_id_t>(), heap.GetInstances(0x012));
    EXPECT_TRUE(heap.InstanceOf(0x001, 0x011));
    EXPECT_TRUE(heap.InstanceOf(0x001, 0x012));
    EXPECT_FALSE(heap.InstanceOf(0x001, 0x013));
}

TEST(heap, gc_root) {
    Heap heap;
    heap.MarkGcRoot(0x001, gc_root_type_t::kRootJniLocal);
    heap.MarkGcRoot(0x002, gc_root_type_t::kRootJniGlobal);
    heap.MarkGcRoot(0x003, gc_root_type_t::kRootJniMonitor);
    std::vector<object_id_t> actual = heap.GetGcRoots();
    std::sort(actual.begin(), actual.end());
    EXPECT_EQ(std::vector<object_id_t>({0x001, 0x002, 0x003}), actual);
    EXPECT_EQ(gc_root_type_t::kRootJniLocal, heap.GetGcRootType(0x001));
    EXPECT_EQ(gc_root_type_t::kRootJniGlobal, heap.GetGcRootType(0x002));
    EXPECT_EQ(gc_root_type_t::kRootJniMonitor, heap.GetGcRootType(0x003));
    EXPECT_THROW(auto i = heap.GetGcRootType(0x004), std::runtime_error);
}

TEST(heap, thread_reference) {
    Heap heap;
    heap.AddThreadReferenceRecord(0x001, 0x201);
    EXPECT_EQ(0x201, heap.GetThreadReference(0x001));
    EXPECT_THROW(auto i = heap.GetThreadReference(0x002), std::runtime_error);
}

TEST(heap, thread_object) {
    Heap heap;
    heap.AddThreadObjectRecord(0x001, 0x201);
    EXPECT_EQ(0x001, heap.GetThreadObject(0x201));
    EXPECT_THROW(auto i = heap.GetThreadObject(0x202), std::runtime_error);
}

TEST(heap, string) {
    Heap heap;
    heap.AddString(0x101, "Hello world!");
    heap.AddString(0x102, "Hello slice!XXX", 12);
    EXPECT_EQ("Hello world!", heap.GetString(0x101));
    EXPECT_EQ(0x101, heap.FindStringId("Hello world!"));
    EXPECT_EQ(0x101, heap.FindStringId("Hello world!")); // Test memorized.
    EXPECT_EQ(0x102, heap.FindStringId("Hello slice!"));
    EXPECT_EQ(0x102, heap.FindStringId("Hello slice!")); // Test memorized.
    EXPECT_THROW(auto i = heap.GetString(0x103), std::runtime_error);
    EXPECT_EQ(std::nullopt, heap.FindStringId("Unknown"));
    EXPECT_EQ(std::nullopt, heap.FindStringId("Unknown")); // Test memorized.
}

TEST(heap, fields_data) {
    Heap heap;
    const uint8_t buffer[] = {0x00, 0x00, 0x00, 0x01, 0x02};
    Reader source_reader(buffer, sizeof(buffer));
    heap.ReadFieldsData(0x001, 0x011, 4, &source_reader);
    const HeapFieldsData *data = heap.ScopedGetFieldsData(0x001);
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(0x001, data->GetInstanceId());
    EXPECT_EQ(0x011, data->GetClassId());
    EXPECT_EQ(4, data->GetSize());
    EXPECT_EQ(1, data->GetScopeReader().ReadU4());
    EXPECT_EQ(2, source_reader.ReadU1());
    EXPECT_EQ(nullptr, heap.ScopedGetFieldsData(0x002));
}

TEST(heap, fields_data_list) {
    Heap heap;
    const uint8_t buffer[] = {0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02};
    Reader source_reader(buffer, sizeof(buffer));
    heap.ReadFieldsData(0x001, 0x011, 4, &source_reader);
    heap.ReadFieldsData(0x002, 0x012, 4, &source_reader);
    std::vector<const HeapFieldsData *> actual = heap.ScopedGetFieldsDataList();
    std::sort(actual.begin(), actual.end(), [](const HeapFieldsData *a, const HeapFieldsData *b) {
        return a->GetInstanceId() < b->GetInstanceId();
    });
    EXPECT_EQ(0x001, actual[0]->GetInstanceId());
    EXPECT_EQ(0x011, actual[0]->GetClassId());
    EXPECT_EQ(4, actual[0]->GetSize());
    EXPECT_EQ(1, actual[0]->GetScopeReader().ReadU4());
    EXPECT_EQ(0x002, actual[1]->GetInstanceId());
    EXPECT_EQ(0x012, actual[1]->GetClassId());
    EXPECT_EQ(4, actual[1]->GetSize());
    EXPECT_EQ(2, actual[1]->GetScopeReader().ReadU4());
}

TEST(heap, primitive_data) {
    Heap heap;
    const uint8_t buffer[] = {
            0x01,   // bool: true
            0x00, 0x61,   // char: 'a'
            0x40, 0x40, 0x00, 0x00,   // float: 3.0
            0x40, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // double: 4.0
            0x05,  // byte: 5
            0x00, 0x06,  // short: 6
            0x00, 0x00, 0x00, 0x07,  // int: 7
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08,  // long: 8
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // unused
    };
    Reader source_reader(buffer, sizeof(buffer));
    heap.ReadPrimitive(0x001, 0x101, value_type_t::kBoolean, &source_reader);
    heap.ReadPrimitive(0x002, 0x102, value_type_t::kChar, &source_reader);
    heap.ReadPrimitive(0x003, 0x103, value_type_t::kFloat, &source_reader);
    heap.ReadPrimitive(0x004, 0x104, value_type_t::kDouble, &source_reader);
    heap.ReadPrimitive(0x005, 0x105, value_type_t::kByte, &source_reader);
    heap.ReadPrimitive(0x006, 0x106, value_type_t::kShort, &source_reader);
    heap.ReadPrimitive(0x007, 0x107, value_type_t::kInt, &source_reader);
    heap.ReadPrimitive(0x008, 0x108, value_type_t::kLong, &source_reader);

    // Boolean.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x001, 0x101);
        EXPECT_EQ(value_type_t::kBoolean, data->GetType());
        EXPECT_EQ(true, data->GetValue<bool>());
    }
    // Char.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x002, 0x102);
        EXPECT_EQ(value_type_t::kChar, data->GetType());
        EXPECT_EQ('a', data->GetValue<char>());
    }
    // Float.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x003, 0x103);
        EXPECT_EQ(value_type_t::kFloat, data->GetType());
        EXPECT_EQ(3.0f, data->GetValue<float>());
    }
    // Double.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x004, 0x104);
        EXPECT_EQ(value_type_t::kDouble, data->GetType());
        EXPECT_EQ(4.0, data->GetValue<double>());
    }
    // Byte.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x005, 0x105);
        EXPECT_EQ(value_type_t::kByte, data->GetType());
        EXPECT_EQ(5, data->GetValue<uint8_t>());
    }
    // Short.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x006, 0x106);
        EXPECT_EQ(value_type_t::kShort, data->GetType());
        EXPECT_EQ(6, data->GetValue<int16_t>());
    }
    // Int.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x007, 0x107);
        EXPECT_EQ(value_type_t::kInt, data->GetType());
        EXPECT_EQ(7, data->GetValue<int32_t>());
    }
    // Long.
    {
        const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x008, 0x108);
        EXPECT_EQ(value_type_t::kLong, data->GetType());
        EXPECT_EQ(8, data->GetValue<int64_t>());
    }
    // Not primitive.
    EXPECT_THROW(HeapPrimitiveData(value_type_t::kObject, &source_reader), std::runtime_error);
}

static bool array_equal(const uint8_t *l, const uint8_t *r, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        if (l[i] != r[i]) {
            return false;
        }
    }
    return true;
}

TEST(heap, primitive_array_data) {
    Heap heap;
    // Random data.
    const uint8_t buffer[] = {
            0xf1, 0xe7, 0x83, 0xe2, 0xc2, 0x4a, 0x39, 0x2d,
            0x86, 0x49, 0x0e, 0x4e, 0xf2, 0x32, 0x02, 0xef,
            0x73, 0x46, 0x87, 0xda, 0x5e, 0xdf, 0xd0, 0x06,
            0xc3, 0x44, 0x50, 0x08, 0xbf, 0x81, 0x26, 0x5b,
            0xdf, 0x77, 0x0b, 0xc5, 0x6d, 0xe4, 0xc1, 0x7d,
            0xe6, 0x7a, 0x06, 0xf4, 0x4a, 0x28, 0x0f, 0xab,
            0xb4, 0x1f, 0x17, 0x72, 0x9c, 0xc4, 0x53, 0x0e,
            0x2f, 0x71, 0xbe, 0xc4, 0x6f, 0x26, 0x85, 0x91
    };
    // Primitive.
    {
        Reader source_reader(buffer, sizeof(buffer));
        heap.ReadPrimitiveArray(0x001, value_type_t::kBoolean, sizeof(buffer), &source_reader);
        const HeapPrimitiveArrayData *data = heap.ScopedGetPrimitiveArrayData(0x001);
        ASSERT_NE(nullptr, data);
        EXPECT_EQ(value_type_t::kBoolean, data->GetType());
        EXPECT_EQ(sizeof(buffer), data->GetSize());
        EXPECT_EQ(64, data->GetLength());
        EXPECT_TRUE(array_equal(buffer, data->GetData(), sizeof(buffer)));
    }
    // Not primitive.
    {
        Reader source_reader(buffer, sizeof(buffer));
        EXPECT_THROW(heap.ReadPrimitiveArray(0x002, value_type_t::kObject, sizeof(buffer), &source_reader),
                     std::runtime_error);
    }
}

TEST(heap, field_primitive) {
    const uint8_t buffer[] = {0xff};
    Heap heap;
    {
        heap.AddString(0x101, "primitive");
        Reader reader(buffer, sizeof(buffer));
        heap.ReadPrimitive(0x001, 0x101, value_type_t::kByte, &reader);
    }
    const HeapPrimitiveData *data = heap.ScopedGetPrimitiveData(0x001, 0x101);
    ASSERT_NE(nullptr, data);
    EXPECT_EQ(value_type_t::kByte, data->GetType());
    EXPECT_EQ(0xff, data->GetValue<uint8_t>());
    EXPECT_EQ(nullptr, heap.ScopedGetPrimitiveData(0x001, 0x102));

    EXPECT_EQ(0xff, heap.GetFieldPrimitive<uint8_t>(0x001, "primitive"));
    EXPECT_EQ(std::nullopt, heap.GetFieldPrimitive<uint8_t>(0x001, "unknown"));
}

TEST(heap, array_primitive) {
    const uint8_t buffer[] = {
            0x00, 0x61, 0x00, 0x62, 0x00, 0x63, 0x00,
            0x64, 0x00, 0x65, 0x00, 0x66
    };
    Reader reader(buffer, sizeof(buffer));

    Heap heap;
    heap.ReadPrimitiveArray(0x001, value_type_t::kChar, sizeof(buffer), &reader);

    const HeapPrimitiveArrayData *data = heap.ScopedGetPrimitiveArrayData(0x001);
    EXPECT_NE(nullptr, data);
    EXPECT_EQ(value_type_t::kChar, data->GetType());
    EXPECT_EQ(sizeof(buffer), data->GetSize());
    EXPECT_EQ(sizeof(buffer) / get_value_type_size(value_type_t::kChar), data->GetLength());

    EXPECT_EQ(std::vector<char>({'a', 'b', 'c', 'd', 'e', 'f'}), heap.GetArrayPrimitive<char>(0x001).value());
    EXPECT_EQ(std::nullopt, heap.GetArrayPrimitive<char>(0x002));
}

TEST(heap, field_reference) {
    Heap heap;
    {
        heap.AddString(0x101, "reference");
        heap.AddString(0x102, "reference_excluded");
        heap.AddString(0x103, "reference_not_recorded");
        heap.AddFieldReference(0x001, 0x101, 0x002);
        heap.AddFieldExcludedReference(0x001, 0x102, 0x003);
    }
    EXPECT_EQ(0x101, heap.GetLeakReferenceGraph().at(0x001).at(0x002).field_name_id);
    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x001).at(0x003), std::out_of_range);

    EXPECT_EQ(0x002, heap.GetFieldReference(0x001, "reference"));
    EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x001, "reference_excluded"));
    EXPECT_EQ(0x003, heap.GetFieldReference(0x001, "reference_excluded", true));
    EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x001, "unknown"));
    EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x001, "reference_not_recorded"));
    EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x002, "reference"));
}

TEST(heap, array_reference) {
    Heap heap;
    heap.AddArrayReference(0x001, 0, 0x002);

    EXPECT_EQ(0, heap.GetLeakReferenceGraph().at(0x001).at(0x002).index);
    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x001).at(0x003), std::out_of_range);

    EXPECT_EQ(0x002, heap.GetArrayReference(0x001, 0));
    EXPECT_EQ(std::nullopt, heap.GetArrayReference(0x001, 1));
    EXPECT_EQ(std::nullopt, heap.GetArrayReference(0x001, 2));
    EXPECT_EQ(std::nullopt, heap.GetArrayReference(0x002, 0));
}

TEST(heap, exclude_references) {
    Heap heap;
    heap.AddString(0x101, "reference");
    heap.AddFieldReference(0x001, 0x101, 0x002);
    EXPECT_NO_THROW(heap.GetLeakReferenceGraph().at(0x001).at(0x002));
    heap.ExcludeReferences(0x001);
    EXPECT_THROW(heap.GetLeakReferenceGraph().at(0x001), std::out_of_range);
    EXPECT_EQ(std::nullopt, heap.GetFieldReference(0x001, "reference"));
    EXPECT_EQ(0x002, heap.GetFieldReference(0x001, "reference", true));
}

TEST(heap, value_from_string_instance) {
    // Below Android API 23.
    {
        Heap heap;
        heap.AddString(0x101, "java.lang.String");
        heap.AddString(0x102, "count");
        heap.AddString(0x103, "offset");
        heap.AddString(0x104, "value");
        heap.AddClassNameRecord(0x011, 0x101);
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x102, .type=value_type_t::kInt});
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x103, .type=value_type_t::kInt});
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x104, .type=value_type_t::kObject});

        const uint8_t fields_data[] = {
                // count: 12
                0x00, 0x00, 0x00, 0x0c,
                // offset: 3
                0x00, 0x00, 0x00, 0x03

        };
        Reader fields_data_reader(fields_data, sizeof(fields_data));
        const uint8_t string_content[] = {
                // prefix: "???"
                0x00, 0x3f, 0x00, 0x3f, 0x00, 0x3f,
                // value: "Hello world!"
                0x00, 0x48, 0x00, 0x65, 0x00, 0x6c, 0x00, 0x6c,
                0x00, 0x6f, 0x00, 0x20, 0x00, 0x77, 0x00, 0x6f,
                0x00, 0x72, 0x00, 0x6c, 0x00, 0x64, 0x00, 0x21
        };
        Reader string_content_reader(string_content, sizeof(string_content));

        heap.ReadPrimitive(0x001, 0x102, value_type_t::kInt, &fields_data_reader);
        heap.ReadPrimitive(0x001, 0x103, value_type_t::kInt, &fields_data_reader);
        heap.AddFieldReference(0x001, 0x104, 0x002);
        heap.AddInstanceClassRecord(0x001, 0x011);

        heap.ReadPrimitiveArray(0x002, value_type_t::kChar, sizeof(string_content), &string_content_reader);

        EXPECT_EQ("Hello world!", heap.GetValueFromStringInstance(0x001).value());
        EXPECT_EQ(std::nullopt, heap.GetValueFromStringInstance(0x003));
    }
    // On and above Android API 23.
    {
        Heap heap;
        heap.AddString(0x101, "java.lang.String");
        heap.AddString(0x102, "test.ChildString");
        heap.AddString(0x103, "value");
        heap.AddClassNameRecord(0x011, 0x101);
        heap.AddClassNameRecord(0x012, 0x102);
        heap.AddInheritanceRecord(0x012, 0x011);
        heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x103, .type=value_type_t::kObject});

        const uint8_t string_content[] = {
                // value: "Hello world!"
                0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20,
                0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21
        };
        Reader string_content_reader(string_content, sizeof(string_content));
        heap.ReadPrimitiveArray(0x003, value_type_t::kByte,
                                sizeof(string_content), &string_content_reader);
        // Self.
        {
            heap.AddInstanceClassRecord(0x001, 0x011);
            heap.AddFieldReference(0x001, 0x103, 0x003);
        }
        // Child.
        {
            heap.AddInstanceClassRecord(0x002, 0x012);
            heap.AddFieldReference(0x002, 0x103, 0x003);
        }

        EXPECT_EQ("Hello world!", heap.GetValueFromStringInstance(0x001).value());
        EXPECT_EQ("Hello world!", heap.GetValueFromStringInstance(0x002).value());
        EXPECT_EQ(std::nullopt, heap.GetValueFromStringInstance(0x003));
    }
    // Error handle.
    {
        // Not child class.
        {
            Heap heap;
            heap.AddString(0x101, "java.lang.String");
            heap.AddString(0x102, "java.lang.Object");
            heap.AddClassNameRecord(0x011, 0x101);
            heap.AddClassNameRecord(0x012, 0x102);
            heap.AddInstanceClassRecord(0x001, 0x012);
            EXPECT_EQ(std::nullopt, heap.GetValueFromStringInstance(0x001));
        }
        // Unknown value array type.
        {
            Heap heap;
            heap.AddString(0x101, "java.lang.String");
            heap.AddString(0x102, "value");
            heap.AddClassNameRecord(0x011, 0x101);
            heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x102, .type=value_type_t::kObject});

            const uint8_t string_content[] = {
                    // value: "Hello world!"
                    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20,
                    0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21
            };
            Reader string_content_reader(string_content, sizeof(string_content));
            heap.ReadPrimitiveArray(0x002, value_type_t::kInt,
                                    sizeof(string_content), &string_content_reader);

            heap.AddInstanceClassRecord(0x001, 0x011);
            heap.AddFieldReference(0x001, 0x102, 0x002);

            EXPECT_THROW(auto i = heap.GetValueFromStringInstance(0x001), std::runtime_error);
        }
    }
}