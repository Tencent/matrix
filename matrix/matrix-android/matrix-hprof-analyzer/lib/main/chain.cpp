#include "internal/main_chain.h"

namespace matrix::hprof {

    const std::string &LeakChain::GcRoot::GetName() const {
        return name_;
    }

    LeakChain::GcRoot::Type LeakChain::GcRoot::GetType() const {
        return type_;
    }

    LeakChain::GcRoot::GcRoot(std::string name, LeakChain::GcRoot::Type type) :
            name_(std::move(name)),
            type_(type) {}

    const std::string &LeakChain::Node::GetReference() const {
        return reference_;
    }

    LeakChain::Node::ReferenceType LeakChain::Node::GetReferenceType() const {
        return reference_type_;
    }

    const std::string &LeakChain::Node::GetObject() const {
        return object_;
    }

    LeakChain::Node::ObjectType LeakChain::Node::GetObjectType() const {
        return object_type_;
    }

    LeakChain::Node::Node(std::string reference, Node::ReferenceType reference_type,
                          std::string object, Node::ObjectType object_type) :
            reference_(std::move(reference)),
            reference_type_(reference_type),
            object_(std::move(object)),
            object_type_(object_type) {}

    const LeakChain::GcRoot &LeakChain::GetGcRoot() const {
        return gc_root_;
    }

    const std::vector<LeakChain::Node> &LeakChain::GetNodes() const {
        return nodes_;
    }

    size_t LeakChain::GetDepth() const {
        return depth_;
    }

    LeakChain::LeakChain(GcRoot gc_root, std::vector<Node> nodes) :
            gc_root_(std::move(gc_root)),
            nodes_(std::move(nodes)),
            depth_(nodes_.size()) {}

    LeakChain::GcRoot create_leak_chain_gc_root(const std::string &name, LeakChain::GcRoot::Type type) {
        return {name, type};
    }

    LeakChain::Node create_leak_chain_node(const std::string &reference, LeakChain::Node::ReferenceType reference_type,
                                           const std::string &object, LeakChain::Node::ObjectType object_type) {
        return {reference, reference_type, object, object_type};
    }

    LeakChain create_leak_chain(const LeakChain::GcRoot &gc_root, const std::vector<LeakChain::Node> &nodes) {
        return {gc_root, nodes};
    }

    LeakChain::GcRoot::Type convert_gc_root_type(internal::heap::gc_root_type_t type) {
        switch (type) {
            case internal::heap::gc_root_type_t::kRootJniGlobal:
                return LeakChain::GcRoot::Type::kRootJniGlobal;
            case internal::heap::gc_root_type_t::kRootJniLocal:
                return LeakChain::GcRoot::Type::kRootJniLocal;
            case internal::heap::gc_root_type_t::kRootJavaFrame:
                return LeakChain::GcRoot::Type::kRootJavaFrame;
            case internal::heap::gc_root_type_t::kRootNativeStack:
                return LeakChain::GcRoot::Type::kRootNativeStack;
            case internal::heap::gc_root_type_t::kRootStickyClass:
                return LeakChain::GcRoot::Type::kRootStickyClass;
            case internal::heap::gc_root_type_t::kRootThreadBlock:
                return LeakChain::GcRoot::Type::kRootThreadBlock;
            case internal::heap::gc_root_type_t::kRootMonitorUsed:
                return LeakChain::GcRoot::Type::kRootMonitorUsed;
            case internal::heap::gc_root_type_t::kRootThreadObject:
                return LeakChain::GcRoot::Type::kRootThreadObject;
            case internal::heap::gc_root_type_t::kRootInternedString:
                return LeakChain::GcRoot::Type::kRootInternedString;
            case internal::heap::gc_root_type_t::kRootFinalizing:
                return LeakChain::GcRoot::Type::kRootFinalizing;
            case internal::heap::gc_root_type_t::kRootDebugger:
                return LeakChain::GcRoot::Type::kRootDebugger;
            case internal::heap::gc_root_type_t::kRootReferenceCleanup:
                return LeakChain::GcRoot::Type::kRootReferenceCleanup;
            case internal::heap::gc_root_type_t::kRootVMInternal:
                return LeakChain::GcRoot::Type::kRootVMInternal;
            case internal::heap::gc_root_type_t::kRootJniMonitor:
                return LeakChain::GcRoot::Type::kRootJniMonitor;
            case internal::heap::gc_root_type_t::kRootUnknown:
                return LeakChain::GcRoot::Type::kRootUnknown;
            case internal::heap::gc_root_type_t::kRootUnreachable:
                return LeakChain::GcRoot::Type::kRootUnreachable;
        }
    }

    LeakChain::Node::ReferenceType convert_reference_type(internal::heap::reference_type_t type) {
        switch (type) {
            case internal::heap::reference_type_t::kStaticField:
                return LeakChain::Node::ReferenceType::kStaticField;
            case internal::heap::reference_type_t::kInstanceField:
                return LeakChain::Node::ReferenceType::kInstanceField;
            case internal::heap::reference_type_t::kArrayElement:
                return LeakChain::Node::ReferenceType::kArrayElement;
        }
    }

    LeakChain::Node::ObjectType convert_object_type(internal::heap::object_type_t type) {
        switch (type) {
            case internal::heap::object_type_t::kClass:
                return LeakChain::Node::ObjectType::kClass;
            case internal::heap::object_type_t::kInstance:
                return LeakChain::Node::ObjectType::kInstance;
            case internal::heap::object_type_t::kObjectArray:
                return LeakChain::Node::ObjectType::kObjectArray;
            case internal::heap::object_type_t::kPrimitiveArray:
                return LeakChain::Node::ObjectType::kPrimitiveArray;
        }
    }
}