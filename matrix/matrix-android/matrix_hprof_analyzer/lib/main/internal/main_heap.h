#ifndef __matrix_hprof_analyzer_internal_heap_h__
#define __matrix_hprof_analyzer_internal_heap_h__

#include "../include/matrix_hprof_analyzer.h"

#include <string>
#include <optional>
#include <vector>

#include "macro.h"

#include "heap.h"

namespace matrix::hprof {

    class HprofHeapImpl {
    public:
        explicit HprofHeapImpl(const internal::heap::Heap &heap);

        [[nodiscard]] std::optional<object_id_t>
        FindClassByName(const std::string &class_name) const;

        [[nodiscard]] std::optional<std::string>
        GetClassName(object_id_t class_id) const;

        [[nodiscard]] std::optional<object_id_t>
        GetSuperClass(object_id_t class_id) const;

        [[nodiscard]] bool ChildClassOf(object_id_t class_id, object_id_t super_class_id) const;

        [[nodiscard]] std::optional<object_id_t>
        GetClass(object_id_t instance_id) const;

        [[nodiscard]] std::vector<object_id_t>
        GetInstances(object_id_t class_id) const;

        [[nodiscard]] bool InstanceOf(object_id_t instance_id, object_id_t class_id) const;

        [[nodiscard]] std::optional<object_id_t>
        GetFieldReference(object_id_t referrer_id, const std::string &field_name) const;

        [[nodiscard]] std::optional<object_id_t>
        GetArrayReference(object_id_t referrer_id, size_t index) const;

        [[nodiscard]] std::optional<uint64_t>
        GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const;

        [[nodiscard]] std::optional<std::vector<uint64_t>>
        GetArrayPrimitiveRaw(object_id_t primitive_array_id) const;

        [[nodiscard]] std::optional<std::string>
        GetValueFromStringInstance(object_id_t instance_id) const;

    private:
        friend_test(main_heap, delegate);

        const internal::heap::Heap &heap_;
    };
}

#endif