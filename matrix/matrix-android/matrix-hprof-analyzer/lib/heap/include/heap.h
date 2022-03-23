#ifndef __matrix_hprof_analyzer_heap_h__
#define __matrix_hprof_analyzer_heap_h__

#include <cstdint>
#include <compare>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include <unistd.h>

#include "macro.h"
#include "reader.h"

namespace matrix::hprof::internal::heap {

    typedef uint64_t object_id_t;
    typedef uint64_t string_id_t;

    typedef uint32_t thread_serial_number_t;

    enum class value_type_t : uint8_t {
        kObject = 2,
        kBoolean = 4,
        kChar = 5,
        kFloat = 6,
        kDouble = 7,
        kByte = 8,
        kShort = 9,
        kInt = 10,
        kLong = 11,
    };

    size_t get_value_type_size(value_type_t type);

    value_type_t value_type_cast(uint8_t type);

    enum class gc_root_type_t {
        kRootJniGlobal,
        kRootJniLocal,
        kRootJavaFrame,
        kRootNativeStack,
        kRootStickyClass,
        kRootThreadBlock,
        kRootMonitorUsed,
        kRootThreadObject,
        kRootInternedString,
        kRootFinalizing,
        kRootDebugger,
        kRootReferenceCleanup,
        kRootVMInternal,
        kRootJniMonitor,
        kRootUnknown,
        kRootUnreachable
    };

    enum class object_type_t {
        kClass,
        kObjectArray,
        kPrimitiveArray,
        kInstance
    };

    enum reference_type_t {
        kStaticField,
        kInstanceField,
        kArrayElement
    };

    struct string_t {
        const char *data;
        size_t length;
    };

    struct reference_t {
        reference_type_t type;
        size_t index;
        string_id_t field_name_id;
    };

    struct field_t {
        string_id_t name_id;
        value_type_t type;

        bool operator==(const field_t& field) const {
            return field.name_id == name_id && field.type == type;
        }
    };

    class HeapFieldsData {
    public:
        HeapFieldsData(object_id_t instance_id,
                       object_id_t class_id,
                       size_t fields_data_size,
                       reader::Reader *source_reader);

        [[nodiscard]] object_id_t GetInstanceId() const {
            return instance_id_;
        }

        [[nodiscard]] object_id_t GetClassId() const {
            return class_id_;
        }

        [[nodiscard]] size_t GetSize() const {
            return fields_data_size_;
        }

        [[nodiscard]] reader::Reader GetScopeReader() const {
            return {fields_data_, fields_data_size_};
        }

    private:
        const object_id_t instance_id_;
        const object_id_t class_id_;
        const size_t fields_data_size_;
        const uint8_t *fields_data_;
    };

    class HeapPrimitiveData {
    public:
        HeapPrimitiveData(value_type_t type, reader::Reader *source_reader);

        [[nodiscard]] value_type_t GetType() const {
            return type_;
        }

        template<typename T>
        [[nodiscard]] T GetValue() const {
            return *reinterpret_cast<const T *>(&data_);
        }

    private:
        const value_type_t type_;
        const uint64_t data_;
    };

    class HeapPrimitiveArrayData {
    public:
        HeapPrimitiveArrayData(value_type_t type, size_t data_size, reader::Reader *source_reader);

        [[nodiscard]] value_type_t GetType() const {
            return type_;
        }

        [[nodiscard]] size_t GetSize() const {
            return data_size_;
        }

        [[nodiscard]] size_t GetLength() const {
            return data_size_ / get_value_type_size(type_);
        }

        [[nodiscard]] const uint8_t *GetData() const {
            return data_;
        }

    private:
        const value_type_t type_;
        const size_t data_size_;
        const uint8_t *data_;
    };

    /**
     * Noticed that all APIs which have the prefix "Find" are not side-effect free. For performance requirements, these
     * functions will memorize the return value and return it again if the same arguments are passed. Though they are
     * marked as const, the state of framework still be changed and memory increases until the analyzing is completed.
     */
    class Heap {
    public:
        ~Heap();

        // identifier size

    public:
        void InitializeIdSize(size_t id_size);

        [[nodiscard]] size_t GetIdSize() const;

    private:
        size_t id_size_ = 0;


        // class name

    public:
        void AddClassNameRecord(object_id_t class_id, string_id_t class_name_id);

        [[nodiscard]] std::optional<string_id_t> GetClassNameId(object_id_t class_id) const;

        [[nodiscard]] std::optional<object_id_t>
        FindClassByName(const std::string &class_name) const;

        [[nodiscard]] std::optional<std::string>
        GetClassName(object_id_t class_id) const;

    private:
        [[nodiscard]] object_id_t FindClassByNameInternal(const std::string &name) const;

        std::map<object_id_t, string_id_t> class_names_;


        // inheritance

    public:
        void AddInheritanceRecord(object_id_t class_id, object_id_t super_class_id);

        [[nodiscard]] std::optional<object_id_t> GetSuperClass(object_id_t class_id) const;

        [[nodiscard]] bool ChildClassOf(object_id_t class_id, object_id_t super_class_id) const;

    private:
        std::map<object_id_t, object_id_t> inheritance_;


        // instance field

    public:
        void AddInstanceFieldRecord(object_id_t class_id, field_t field);

        [[nodiscard]] const std::vector<field_t> &GetInstanceFields(object_id_t class_id) const;

    private:
        std::map<object_id_t, std::vector<field_t>> instance_fields_;


        // instance type

    public:
        void AddInstanceTypeRecord(object_id_t instance_id, object_type_t type);

        [[nodiscard]] object_type_t GetInstanceType(object_id_t instance_id) const;

    private:
        std::map<object_id_t, object_type_t> instance_types_;


        // instance class

    public:
        void AddInstanceClassRecord(object_id_t instance_id, object_id_t class_id);

        [[nodiscard]] std::optional<object_id_t> GetClass(object_id_t instance_id) const;

        [[nodiscard]] std::vector<object_id_t> GetInstances(object_id_t class_id) const;

        [[nodiscard]] bool InstanceOf(object_id_t instance_id, object_id_t class_id) const;

    private:
        std::map<object_id_t, object_id_t> instance_classes_;


        // GC root

    public:
        void MarkGcRoot(object_id_t instance_id, gc_root_type_t type);

        [[nodiscard]] gc_root_type_t GetGcRootType(object_id_t gc_root) const;

        [[nodiscard]] const std::vector<object_id_t> &GetGcRoots() const;

    private:
        std::vector<object_id_t> gc_roots_;
        std::map<object_id_t, gc_root_type_t> gc_root_types_;


        // thread

    public:
        void AddThreadReferenceRecord(object_id_t instance_id, thread_serial_number_t thread_serial_number);

        [[nodiscard]] thread_serial_number_t GetThreadReference(object_id_t instance_id) const;

        void AddThreadObjectRecord(object_id_t thread_object_id, thread_serial_number_t thread_serial_number);

        [[nodiscard]] object_id_t GetThreadObject(thread_serial_number_t thread_serial_number) const;

    private:
        std::map<object_id_t, thread_serial_number_t> thread_serial_numbers_;
        std::map<thread_serial_number_t, object_id_t> thread_object_ids_;


        // string

    public:
        void AddString(string_id_t string_id, const char *data) {
            AddString(string_id, data, strlen(data));
        }

        void AddString(string_id_t string_id, const char *data, size_t length);

        [[nodiscard]] std::optional<std::string> GetString(string_id_t string_id) const;

        [[nodiscard]] std::optional<string_id_t> FindStringId(const std::string &value) const;

    private:
        [[nodiscard]] string_id_t FindStringIdInternal(const std::string &value) const;

        std::map<string_id_t, string_t> strings_;


        // fields data

    public:
        void ReadFieldsData(object_id_t instance_id, object_id_t class_id,
                            size_t fields_data_size, reader::Reader *source_reader);

        [[nodiscard]] const HeapFieldsData *ScopedGetFieldsData(object_id_t instance_id) const;

        [[nodiscard]] std::vector<const HeapFieldsData *> ScopedGetFieldsDataList() const;

    private:
        std::map<object_id_t, HeapFieldsData> fields_data_;


        // primitive

    public:
        void ReadPrimitive(object_id_t referrer_id, string_id_t field_name_id,
                           value_type_t type, reader::Reader *source_reader);

        void ReadPrimitiveArray(object_id_t primitive_array_object_id,
                                value_type_t type, size_t data_size, reader::Reader *source_reader);

        [[nodiscard]] const HeapPrimitiveData *
        ScopedGetPrimitiveData(object_id_t instance_id, string_id_t field_name_id) const;

        [[nodiscard]] const HeapPrimitiveArrayData *
        ScopedGetPrimitiveArrayData(object_id_t primitive_array_object_id) const;

        template<typename T>
        [[nodiscard]] std::optional<T>
        GetFieldPrimitive(object_id_t referrer_id, const std::string &reference_name) const {
            const uint64_t data = unwrap(GetFieldPrimitiveRaw(referrer_id, reference_name), return std::nullopt);
            return *reinterpret_cast<const T *>(&data);
        }

        [[nodiscard]] std::optional<uint64_t>
        GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const;

        template<typename T>
        [[nodiscard]] std::optional<std::vector<T>>
        GetArrayPrimitive(object_id_t primitive_array_id) const {
            const std::vector<uint64_t> data_list = unwrap(GetArrayPrimitiveRaw(primitive_array_id),
                                                           return std::nullopt);
            std::vector<T> result;
            for (const auto data: data_list) {
                result.template emplace_back(*reinterpret_cast<const T *>(&data));
            }
            return result;
        }

        [[nodiscard]] std::optional<std::vector<uint64_t>>
        GetArrayPrimitiveRaw(object_id_t primitive_array_id) const;

    private:
        std::map<object_id_t, std::map<string_id_t, HeapPrimitiveData>> primitives_;
        std::map<object_id_t, HeapPrimitiveArrayData> primitive_arrays_;


        // reference

    public:
        void AddFieldReference(object_id_t referrer_id, string_id_t field_name_id, object_id_t referent_id,
                               bool static_reference = false);

        /**
         * See ::AddExcludedReference.
         */
        void AddFieldExcludedReference(object_id_t referrer_id, string_id_t field_name_id, object_id_t referent_id,
                                       bool static_reference = false);

        [[nodiscard]] std::optional<object_id_t>
        GetFieldReference(object_id_t referrer_id, const std::string &reference_name, bool with_excluded = false) const;

        void AddArrayReference(object_id_t referrer_id, size_t index, object_id_t referent_id);

        [[nodiscard]] std::optional<object_id_t>
        GetArrayReference(object_id_t referrer_id, size_t index) const;

        void ExcludeReferences(object_id_t referrer_id);

        /**
         * The directed graph of references not being excluded. The analyzer can ignore exclude reference matchers and
         * calculate shortest path with graph directly for increasing performance.
         */
        [[nodiscard]] const std::map<object_id_t, std::map<object_id_t, reference_t>> &GetLeakReferenceGraph() const;

    private:
        void AddReference(object_id_t referrer_id, reference_t reference, object_id_t referent_id);

        /**
         * Add a reference which is matched by exclude matchers while parsing heap. The reference will not be added to
         * leak reference graph, but it can still be searched by ::GetFieldReference if \a with_excluded is true so that
         * API caller can use the whole reference graph to find what they want.
         */
        void AddExcludedReference(object_id_t referrer_id, reference_t reference, object_id_t referent_id);

        std::map<object_id_t, std::map<object_id_t, reference_t>> references_;
        std::map<object_id_t, std::map<object_id_t, reference_t>> excluded_references_;


        // tools

    public:

        [[nodiscard]] std::optional<std::string>
        GetValueFromStringInstance(object_id_t instance_id) const;

    private:

        static const std::vector<field_t> empty_fields_;
    };
}

#endif