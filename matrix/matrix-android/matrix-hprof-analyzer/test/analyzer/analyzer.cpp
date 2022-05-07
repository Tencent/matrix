#include "analyzer.h"
#include "gtest/gtest.h"

using namespace matrix::hprof::internal::analyzer;
using namespace matrix::hprof::internal::heap;

TEST(analyzer, find_leak_chain) {
    Heap heap;
    {
        heap.AddFieldReference(0x001, 0x101, 0x002);
        heap.AddFieldReference(0x002, 0x102, 0x003);
        heap.AddFieldReference(0x003, 0x103, 0x004);
        heap.AddFieldReference(0x004, 0x104, 0x005);
        heap.AddFieldReference(0x006, 0x105, 0x007);
        heap.AddFieldReference(0x007, 0x106, 0x008);
        heap.AddFieldReference(0x008, 0x107, 0x007);
        heap.AddFieldReference(0x008, 0x108, 0x005);
        heap.AddFieldReference(0x001, 0x109, 0x009);
        heap.AddFieldReference(0x009, 0x10a, 0x00a);

        heap.MarkGcRoot(0x001, gc_root_type_t::kRootUnknown);
        heap.MarkGcRoot(0x006, gc_root_type_t::kRootUnknown);
    }

    const std::map<object_id_t, std::vector<std::pair<object_id_t, std::optional<reference_t>>>> result =
            find_leak_chains(heap, {0x005, 0x00a, 0x00b});

    { // Chain of 0x005 (including circle references).
        EXPECT_EQ(0x006, result.at(0x005)[0].first);
        EXPECT_EQ(0x105, result.at(0x005)[0].second.value().field_name_id);

        EXPECT_EQ(0x007, result.at(0x005)[1].first);
        EXPECT_EQ(0x106, result.at(0x005)[1].second.value().field_name_id);

        EXPECT_EQ(0x008, result.at(0x005)[2].first);
        EXPECT_EQ(0x108, result.at(0x005)[2].second.value().field_name_id);

        EXPECT_EQ(0x005, result.at(0x005)[3].first);
        EXPECT_EQ(std::nullopt, result.at(0x005)[3].second);
    }

    { // Chain of 0x00a.
        EXPECT_EQ(0x001, result.at(0x00a)[0].first);
        EXPECT_EQ(0x109, result.at(0x00a)[0].second.value().field_name_id);

        EXPECT_EQ(0x009, result.at(0x00a)[1].first);
        EXPECT_EQ(0x10a, result.at(0x00a)[1].second.value().field_name_id);

        EXPECT_EQ(0x00a, result.at(0x00a)[2].first);
        EXPECT_EQ(std::nullopt, result.at(0x00a)[2].second);
    }

    { // Chain of 0x009 (in graph but not a target leak).
        EXPECT_THROW(result.at(0x009), std::out_of_range);
    }

    { // Chain of 0x00b (not in graph).
        EXPECT_THROW(result.at(0x00b), std::out_of_range);
    }
}