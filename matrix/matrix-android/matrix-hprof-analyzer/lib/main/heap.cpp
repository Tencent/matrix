#include "internal/main_heap.h"

namespace matrix::hprof {

    // public interface

    HprofHeap::HprofHeap(HprofHeapImpl *impl) : impl_(impl) {}

    std::optional<object_id_t> HprofHeap::FindClassByName(const std::string &class_name) const {
        return impl_->FindClassByName(class_name);
    }

    std::optional<std::string> HprofHeap::GetClassName(object_id_t class_id) const {
        return impl_->GetClassName(class_id);
    }

    std::optional<object_id_t> HprofHeap::GetSuperClass(object_id_t class_id) const {
        return impl_->GetSuperClass(class_id);
    }

    bool HprofHeap::ChildClassOf(object_id_t class_id, object_id_t super_class_id) const {
        return impl_->ChildClassOf(class_id, super_class_id);
    }

    std::optional<object_id_t> HprofHeap::GetClass(object_id_t instance_id) const {
        return impl_->GetClass(instance_id);
    }

    std::vector<object_id_t> HprofHeap::GetInstances(object_id_t class_id) const {
        return impl_->GetInstances(class_id);
    }

    bool HprofHeap::InstanceOf(object_id_t instance_id, object_id_t class_id) const {
        return impl_->InstanceOf(instance_id, class_id);
    }

    std::optional<object_id_t>
    HprofHeap::GetFieldReference(object_id_t referrer_id, const std::string &field_name) const {
        return impl_->GetFieldReference(referrer_id, field_name);
    }

    std::optional<object_id_t>
    HprofHeap::GetArrayReference(object_id_t referrer_id, size_t index) const {
        return impl_->GetArrayReference(referrer_id, index);
    }

    std::optional<uint64_t>
    HprofHeap::GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const {
        return impl_->GetFieldPrimitiveRaw(referrer_id, reference_name);
    }

    std::optional<std::vector<uint64_t>>
    HprofHeap::GetArrayPrimitiveRaw(object_id_t primitive_array_id) const {
        return impl_->GetArrayPrimitiveRaw(primitive_array_id);
    }

    std::optional<std::string> HprofHeap::GetValueFromStringInstance(object_id_t instance_id) const {
        return impl_->GetValueFromStringInstance(instance_id);
    }

    // implementation

    HprofHeapImpl::HprofHeapImpl(const internal::heap::Heap &heap) : heap_(heap) {}

    std::optional<object_id_t> HprofHeapImpl::FindClassByName(const std::string &class_name) const {
        return heap_.FindClassByName(class_name);
    }

    std::optional<std::string> HprofHeapImpl::GetClassName(object_id_t class_id) const {
        return heap_.GetClassName(class_id);
    }

    std::optional<object_id_t> HprofHeapImpl::GetSuperClass(object_id_t class_id) const {
        return heap_.GetSuperClass(class_id);
    }

    bool HprofHeapImpl::ChildClassOf(object_id_t class_id, object_id_t super_class_id) const {
        return heap_.ChildClassOf(class_id, super_class_id);
    }

    std::optional<object_id_t> HprofHeapImpl::GetClass(object_id_t instance_id) const {
        return heap_.GetClass(instance_id);
    }

    std::vector<object_id_t> HprofHeapImpl::GetInstances(object_id_t class_id) const {
        return heap_.GetInstances(class_id);
    }

    bool HprofHeapImpl::InstanceOf(object_id_t instance_id, object_id_t class_id) const {
        return heap_.InstanceOf(instance_id, class_id);
    }

    std::optional<object_id_t> HprofHeapImpl::GetFieldReference(object_id_t referrer_id,
                                                                const std::string &field_name) const {
        return heap_.GetFieldReference(referrer_id, field_name, true);
    }

    std::optional<object_id_t> HprofHeapImpl::GetArrayReference(object_id_t referrer_id, size_t index) const {
        return heap_.GetArrayReference(referrer_id, index);
    }

    std::optional<uint64_t>
    HprofHeapImpl::GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const {
        return heap_.GetFieldPrimitiveRaw(referrer_id, reference_name);
    }

    std::optional<std::vector<uint64_t>>
    HprofHeapImpl::GetArrayPrimitiveRaw(object_id_t primitive_array_id) const {
        return heap_.GetArrayPrimitiveRaw(primitive_array_id);
    }

    std::optional<std::string> HprofHeapImpl::GetValueFromStringInstance(object_id_t instance_id) const {
        return heap_.GetValueFromStringInstance(instance_id);
    }
}