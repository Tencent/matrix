#include "main_heap.h"
#include "gtest/gtest.h"

using namespace matrix::hprof::internal::heap;

namespace matrix::hprof {

    TEST(main_heap, delegate) {
        {
            Heap heap;
            heap.AddString(0x111, "test.SampleClass");
            heap.AddClassNameRecord(0x011, 0x111);
            heap.AddInheritanceRecord(0x011, 0x012);
            heap.AddInstanceClassRecord(0x001, 0x011);

            HprofHeap hprof_heap(new HprofHeapImpl(heap));
            EXPECT_EQ(0x011, hprof_heap.FindClassByName("test.SampleClass"));
            EXPECT_EQ("test.SampleClass", hprof_heap.GetClassName(0x011));
            EXPECT_EQ(0x012, hprof_heap.GetSuperClass(0x011));
            EXPECT_TRUE(hprof_heap.ChildClassOf(0x011, 0x012));
        }
        {
            Heap heap;
            heap.AddInheritanceRecord(0x011, 0x012);
            heap.AddInstanceClassRecord(0x001, 0x011);

            HprofHeap hprof_heap(new HprofHeapImpl(heap));
            EXPECT_EQ(0x011, hprof_heap.GetClass(0x001));
            EXPECT_EQ(std::vector<object_id_t>{0x001}, hprof_heap.GetInstances(0x011));
            EXPECT_TRUE(hprof_heap.InstanceOf(0x001, 0x012));
        }
        {
            Heap heap;
            heap.AddString(0x101, "reference");
            heap.AddString(0x102, "referenceExclude");
            heap.AddFieldReference(0x001, 0x101, 0x002);
            heap.AddFieldExcludedReference(0x001, 0x102, 0x003);
            heap.AddArrayReference(0x002, 0, 0x004);

            HprofHeap hprof_heap(new HprofHeapImpl(heap));
            EXPECT_EQ(0x002, hprof_heap.GetFieldReference(0x001, "reference"));
            EXPECT_EQ(0x003, hprof_heap.GetFieldReference(0x001, "referenceExclude"));
            EXPECT_EQ(0x004, hprof_heap.GetArrayReference(0x002, 0));
        }
        {
            const uint8_t buffer[] = {0x1};
            internal::reader::Reader reader(buffer, sizeof(buffer));
            Heap heap;
            heap.AddString(0x101, "primitive");
            heap.ReadPrimitive(0x001, 0x101, value_type_t::kBoolean, &reader);
            HprofHeap hprof_heap(new HprofHeapImpl(heap));

            EXPECT_EQ(true, hprof_heap.GetFieldPrimitive<bool>(0x001, "primitive"));
        }
        {
            const uint8_t buffer[] = {0x0, 0x1, 0x0, 0x2, 0x0, 0x3};
            internal::reader::Reader reader(buffer, sizeof(buffer));
            Heap heap;
            heap.AddString(0x101, "primitive");
            heap.ReadPrimitiveArray(0x001, value_type_t::kShort, sizeof(buffer), &reader);

            HprofHeap hprof_heap(new HprofHeapImpl(heap));
            EXPECT_EQ(std::vector<short>({1, 2, 3}), hprof_heap.GetArrayPrimitive<short>(0x001));
        }
        {
            Heap heap;
            heap.AddString(0x101, "java.lang.String");
            heap.AddString(0x103, "value");
            heap.AddClassNameRecord(0x011, 0x101);
            heap.AddInstanceFieldRecord(0x011, field_t{.name_id=0x103, .type=value_type_t::kObject});
            heap.AddInstanceClassRecord(0x001, 0x011);
            heap.AddFieldReference(0x001, 0x103, 0x003);

            const uint8_t string_content[] = {
                    // value: "Hello world!"
                    0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20,
                    0x77, 0x6f, 0x72, 0x6c, 0x64, 0x21
            };
            internal::reader::Reader string_content_reader(string_content, sizeof(string_content));
            heap.ReadPrimitiveArray(0x003, value_type_t::kByte,
                                    sizeof(string_content), &string_content_reader);

            HprofHeap hprof_heap(new HprofHeapImpl(heap));
            EXPECT_EQ("Hello world!", hprof_heap.GetValueFromStringInstance(0x001).value());
        }
    }
}