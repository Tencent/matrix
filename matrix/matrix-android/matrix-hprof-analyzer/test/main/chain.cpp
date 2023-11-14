#include "main_chain.h"
#include "gtest/gtest.h"

#include "heap.h"

using namespace matrix::hprof;
using namespace matrix::hprof::internal::heap;

TEST(main_chain, convert_gc_root_type) {
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootJniGlobal,
              convert_gc_root_type(gc_root_type_t::kRootJniGlobal));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootJniLocal,
              convert_gc_root_type(gc_root_type_t::kRootJniLocal));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootJavaFrame,
              convert_gc_root_type(gc_root_type_t::kRootJavaFrame));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootNativeStack,
              convert_gc_root_type(gc_root_type_t::kRootNativeStack));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootStickyClass,
              convert_gc_root_type(gc_root_type_t::kRootStickyClass));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootThreadBlock,
              convert_gc_root_type(gc_root_type_t::kRootThreadBlock));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootMonitorUsed,
              convert_gc_root_type(gc_root_type_t::kRootMonitorUsed));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootThreadObject,
              convert_gc_root_type(gc_root_type_t::kRootThreadObject));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootInternedString,
              convert_gc_root_type(gc_root_type_t::kRootInternedString));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootFinalizing,
              convert_gc_root_type(gc_root_type_t::kRootFinalizing));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootDebugger,
              convert_gc_root_type(gc_root_type_t::kRootDebugger));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootReferenceCleanup,
              convert_gc_root_type(gc_root_type_t::kRootReferenceCleanup));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootVMInternal,
              convert_gc_root_type(gc_root_type_t::kRootVMInternal));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootJniMonitor,
              convert_gc_root_type(gc_root_type_t::kRootJniMonitor));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootUnknown,
              convert_gc_root_type(gc_root_type_t::kRootUnknown));
    EXPECT_EQ(LeakChain::GcRoot::Type::kRootUnreachable,
              convert_gc_root_type(gc_root_type_t::kRootUnreachable));
}

TEST(main_chain, convert_reference_type) {
    EXPECT_EQ(LeakChain::Node::ReferenceType::kStaticField,
              convert_reference_type(reference_type_t::kStaticField));
    EXPECT_EQ(LeakChain::Node::ReferenceType::kInstanceField,
              convert_reference_type(reference_type_t::kInstanceField));
    EXPECT_EQ(LeakChain::Node::ReferenceType::kArrayElement,
              convert_reference_type(reference_type_t::kArrayElement));
}

TEST(main_chain, convert_object_type) {
    EXPECT_EQ(LeakChain::Node::ObjectType::kClass,
              convert_object_type(object_type_t::kClass));
    EXPECT_EQ(LeakChain::Node::ObjectType::kInstance,
              convert_object_type(object_type_t::kInstance));
    EXPECT_EQ(LeakChain::Node::ObjectType::kObjectArray,
              convert_object_type(object_type_t::kObjectArray));
    EXPECT_EQ(LeakChain::Node::ObjectType::kPrimitiveArray,
              convert_object_type(object_type_t::kPrimitiveArray));
}