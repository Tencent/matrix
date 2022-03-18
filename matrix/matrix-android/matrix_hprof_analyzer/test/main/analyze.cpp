#include "main_analyzer.h"
#include "gtest/gtest.h"

#include <cstdio>
#include <fcntl.h>

#include "heap.h"

#include "errorha.h"

using namespace matrix::hprof::internal::heap;

namespace matrix::hprof {

    TEST(main_analyzer, construct) {
        const uint8_t buffer[] = "magic";
        int fd = fileno(tmpfile());
        write(fd, buffer, sizeof(buffer));

        HprofAnalyzer analyzer(fd);
        HprofAnalyzerImpl *impl = analyzer.impl_.get();

        EXPECT_EQ(sizeof(buffer), impl->data_size_);
        ASSERT_NE(nullptr, impl->data_);
        EXPECT_TRUE(strcmp(reinterpret_cast<const char *>(buffer), reinterpret_cast<const char *>(impl->data_)) == 0);

        close(fd);
    }

    TEST(main_analyzer, construct_error_handle) {
        // unknown file descriptor
        {
            HprofAnalyzer impl(-1);
            EXPECT_EQ(std::nullopt, impl.Analyze([](const HprofHeap &) { return std::vector<matrix::hprof::object_id_t>(); }));
            std::cout << get_matrix_hprof_analyzer_error() << std::endl;
            EXPECT_EQ(0, strcmp("Failed to invoke fstat with errno 9.", get_matrix_hprof_analyzer_error()));
        }
        // not regular file
        {
            char name_template[] = "matrix-hprof-test-temp-XXXXXX";
            char *temp_dir_path = mkdtemp(name_template);
            if (temp_dir_path == nullptr) FAIL() << "Unsupported test platform: Failed to create temporary directory.";
            int fd = open(temp_dir_path, O_RDONLY);
            HprofAnalyzer impl(fd);
            EXPECT_EQ(std::nullopt, impl.Analyze([](const HprofHeap &) { return std::vector<matrix::hprof::object_id_t>(); }));
            EXPECT_EQ(0, strcmp("File descriptor is not a regular file.", get_matrix_hprof_analyzer_error()));
        }
    }

    TEST(main_analyzer, exclude_matchers) {
        // instance field
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeInstanceFieldReference("test.SampleClass", "reference");
            EXPECT_FALSE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchClassName());
            EXPECT_EQ("test.SampleClass", impl->exclude_matcher_group_.instance_fields_[0].GetClassName());
            EXPECT_FALSE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchFieldName());
            EXPECT_EQ("reference", impl->exclude_matcher_group_.instance_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeInstanceFieldReference("test.SampleClass", "*");
            EXPECT_FALSE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchClassName());
            EXPECT_EQ("test.SampleClass", impl->exclude_matcher_group_.instance_fields_[0].GetClassName());
            EXPECT_TRUE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchFieldName());
            EXPECT_EQ("", impl->exclude_matcher_group_.instance_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeInstanceFieldReference("*", "reference");
            EXPECT_TRUE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchClassName());
            EXPECT_EQ("", impl->exclude_matcher_group_.instance_fields_[0].GetClassName());
            EXPECT_FALSE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchFieldName());
            EXPECT_EQ("reference", impl->exclude_matcher_group_.instance_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeInstanceFieldReference("*", "*");
            EXPECT_TRUE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchClassName());
            EXPECT_EQ("", impl->exclude_matcher_group_.instance_fields_[0].GetClassName());
            EXPECT_TRUE(impl->exclude_matcher_group_.instance_fields_[0].FullMatchFieldName());
            EXPECT_EQ("", impl->exclude_matcher_group_.instance_fields_[0].GetFieldName());
        }
        // static field
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeStaticFieldReference("test.SampleClass", "reference");
            EXPECT_FALSE(impl->exclude_matcher_group_.static_fields_[0].FullMatchClassName());
            EXPECT_EQ("test.SampleClass", impl->exclude_matcher_group_.static_fields_[0].GetClassName());
            EXPECT_FALSE(impl->exclude_matcher_group_.static_fields_[0].FullMatchFieldName());
            EXPECT_EQ("reference", impl->exclude_matcher_group_.static_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeStaticFieldReference("test.SampleClass", "*");
            EXPECT_FALSE(impl->exclude_matcher_group_.static_fields_[0].FullMatchClassName());
            EXPECT_EQ("test.SampleClass", impl->exclude_matcher_group_.static_fields_[0].GetClassName());
            EXPECT_TRUE(impl->exclude_matcher_group_.static_fields_[0].FullMatchFieldName());
            EXPECT_EQ("", impl->exclude_matcher_group_.static_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeStaticFieldReference("*", "reference");
            EXPECT_TRUE(impl->exclude_matcher_group_.static_fields_[0].FullMatchClassName());
            EXPECT_EQ("", impl->exclude_matcher_group_.static_fields_[0].GetClassName());
            EXPECT_FALSE(impl->exclude_matcher_group_.static_fields_[0].FullMatchFieldName());
            EXPECT_EQ("reference", impl->exclude_matcher_group_.static_fields_[0].GetFieldName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeStaticFieldReference("*", "*");
            EXPECT_TRUE(impl->exclude_matcher_group_.static_fields_[0].FullMatchClassName());
            EXPECT_EQ("", impl->exclude_matcher_group_.static_fields_[0].GetClassName());
            EXPECT_TRUE(impl->exclude_matcher_group_.static_fields_[0].FullMatchFieldName());
            EXPECT_EQ("", impl->exclude_matcher_group_.static_fields_[0].GetFieldName());
        }
        // thread
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeThreadReference("sample_thread");
            EXPECT_FALSE(impl->exclude_matcher_group_.threads_[0].FullMatchThreadName());
            EXPECT_EQ("sample_thread", impl->exclude_matcher_group_.threads_[0].GetThreadName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeThreadReference("*");
            EXPECT_TRUE(impl->exclude_matcher_group_.threads_[0].FullMatchThreadName());
            EXPECT_EQ("", impl->exclude_matcher_group_.threads_[0].GetThreadName());
        }
        // native global
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeNativeGlobalReference("test.SampleClass");
            EXPECT_FALSE(impl->exclude_matcher_group_.native_globals_[0].FullMatchClassName());
            EXPECT_EQ("test.SampleClass", impl->exclude_matcher_group_.native_globals_[0].GetClassName());
        }
        {
            int fd = fileno(tmpfile());
            HprofAnalyzer analyzer(fd);
            HprofAnalyzerImpl *impl = analyzer.impl_.get();
            analyzer.ExcludeNativeGlobalReference("*");
            EXPECT_TRUE(impl->exclude_matcher_group_.native_globals_[0].FullMatchClassName());
            EXPECT_EQ("", impl->exclude_matcher_group_.native_globals_[0].GetClassName());
        }
    }

    TEST(main_analyzer_impl, analyzerlyze) {
        Heap heap;
        {
            heap.AddString(0x111, "test.Root");
            heap.AddClassNameRecord(0x011, 0x111);
            heap.AddInstanceClassRecord(0x001, 0x011);
            heap.MarkGcRoot(0x001, gc_root_type_t::kRootJavaFrame);
            heap.AddInstanceTypeRecord(0x001, object_type_t::kInstance);

            heap.AddString(0x112, "test.NodeClass");
            heap.AddClassNameRecord(0x012, 0x112);
            heap.AddInstanceTypeRecord(0x012, object_type_t::kClass);

            heap.AddString(0x113, "test.Leak[]");
            heap.AddClassNameRecord(0x013, 0x113);
            heap.AddInstanceClassRecord(0x003, 0x013);
            heap.AddInstanceTypeRecord(0x003, object_type_t::kObjectArray);

            heap.AddString(0x114, "test.Leak");
            heap.AddClassNameRecord(0x014, 0x114);
            heap.AddInstanceClassRecord(0x004, 0x014);
            heap.AddInstanceTypeRecord(0x004, object_type_t::kInstance);

            heap.AddString(0x101, "instanceRef");
            heap.AddString(0x102, "staticRef");

            heap.AddFieldReference(0x001, 0x101, 0x012);
            heap.AddFieldReference(0x012, 0x102, 0x003, true);
            heap.AddArrayReference(0x003, 0, 0x004);
        }

        std::vector<std::pair<object_id_t, std::optional<reference_t>>> chain;
        chain.emplace_back(0x001, reference_t{.type = internal::heap::kInstanceField, .field_name_id = 0x101});
        chain.emplace_back(0x002, reference_t{.type = internal::heap::kStaticField, .field_name_id = 0x102});
        chain.emplace_back(0x003, reference_t{.type = internal::heap::kArrayElement, .index = 0});
        chain.emplace_back(0x004, std::nullopt);

        std::vector<LeakChain> leak_chains = HprofAnalyzerImpl::Analyze(heap, {0x003, 0x004, 0x005});
        std::sort(leak_chains.begin(), leak_chains.end(), [](const LeakChain &lhs, const LeakChain &rhs) {
            return lhs.GetDepth() < lhs.GetDepth();
        });
        ASSERT_EQ(2, leak_chains.size());

        {
            const LeakChain &leak_chain = leak_chains[0];

            EXPECT_EQ(LeakChain::GcRoot::Type::kRootJavaFrame, leak_chain.GetGcRoot().GetType());
            EXPECT_EQ("test.Root", leak_chain.GetGcRoot().GetName());

            EXPECT_EQ(LeakChain::Node::ObjectType::kClass, leak_chain.GetNodes()[0].GetObjectType());
            EXPECT_EQ("test.NodeClass", leak_chain.GetNodes()[0].GetObject());
            EXPECT_EQ(LeakChain::Node::ReferenceType::kInstanceField, leak_chain.GetNodes()[0].GetReferenceType());
            EXPECT_EQ("instanceRef", leak_chain.GetNodes()[0].GetReference());

            EXPECT_EQ(LeakChain::Node::ObjectType::kObjectArray, leak_chain.GetNodes()[1].GetObjectType());
            EXPECT_EQ("test.Leak[]", leak_chain.GetNodes()[1].GetObject());
            EXPECT_EQ(LeakChain::Node::ReferenceType::kStaticField, leak_chain.GetNodes()[1].GetReferenceType());
            EXPECT_EQ("staticRef", leak_chain.GetNodes()[1].GetReference());
        }

        {
            const LeakChain &leak_chain = leak_chains[1];

            EXPECT_EQ(LeakChain::GcRoot::Type::kRootJavaFrame, leak_chain.GetGcRoot().GetType());
            EXPECT_EQ("test.Root", leak_chain.GetGcRoot().GetName());

            EXPECT_EQ(LeakChain::Node::ObjectType::kClass, leak_chain.GetNodes()[0].GetObjectType());
            EXPECT_EQ("test.NodeClass", leak_chain.GetNodes()[0].GetObject());
            EXPECT_EQ(LeakChain::Node::ReferenceType::kInstanceField, leak_chain.GetNodes()[0].GetReferenceType());
            EXPECT_EQ("instanceRef", leak_chain.GetNodes()[0].GetReference());

            EXPECT_EQ(LeakChain::Node::ObjectType::kObjectArray, leak_chain.GetNodes()[1].GetObjectType());
            EXPECT_EQ("test.Leak[]", leak_chain.GetNodes()[1].GetObject());
            EXPECT_EQ(LeakChain::Node::ReferenceType::kStaticField, leak_chain.GetNodes()[1].GetReferenceType());
            EXPECT_EQ("staticRef", leak_chain.GetNodes()[1].GetReference());

            EXPECT_EQ(LeakChain::Node::ObjectType::kInstance, leak_chain.GetNodes()[2].GetObjectType());
            EXPECT_EQ("test.Leak", leak_chain.GetNodes()[2].GetObject());
            EXPECT_EQ(LeakChain::Node::ReferenceType::kArrayElement, leak_chain.GetNodes()[2].GetReferenceType());
            EXPECT_EQ("0", leak_chain.GetNodes()[2].GetReference());
        }
    }
}