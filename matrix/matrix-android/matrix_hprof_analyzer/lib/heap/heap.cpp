#include "include/heap.h"

#include <codecvt>
#include <locale>

namespace matrix::hprof::internal::heap {

    size_t get_value_type_size(value_type_t type) {
        switch (type) {
            case value_type_t::kObject:
                return 0;
            case value_type_t::kBoolean:
                return sizeof(uint8_t);
            case value_type_t::kChar:
                // See https://www.oracle.com/technical-resources/articles/javase/supplementary.html.
                return sizeof(uint16_t);
            case value_type_t::kFloat:
                return sizeof(uint32_t);
            case value_type_t::kDouble:
                return sizeof(uint64_t);
            case value_type_t::kByte:
                return sizeof(uint8_t);
            case value_type_t::kShort:
                return sizeof(uint16_t);
            case value_type_t::kInt:
                return sizeof(uint32_t);
            case value_type_t::kLong:
                return sizeof(uint64_t);
        }
    }

    static const std::vector<uint8_t> valid_value_types = {
            static_cast<const uint8_t>(value_type_t::kObject),
            static_cast<const uint8_t>(value_type_t::kBoolean),
            static_cast<const uint8_t>(value_type_t::kChar),
            static_cast<const uint8_t>(value_type_t::kFloat),
            static_cast<const uint8_t>(value_type_t::kDouble),
            static_cast<const uint8_t>(value_type_t::kByte),
            static_cast<const uint8_t>(value_type_t::kShort),
            static_cast<const uint8_t>(value_type_t::kInt),
            static_cast<const uint8_t>(value_type_t::kLong)
    };

    value_type_t value_type_cast(uint8_t type) {
        if (std::find(valid_value_types.begin(), valid_value_types.end(), type) ==
            valid_value_types.end()) {
            throw std::runtime_error("Invalid value type." + std::to_string(type));
        }
        return static_cast<value_type_t>(type);
    }

    HeapFieldsData::HeapFieldsData(object_id_t instance_id, object_id_t class_id,
                                   size_t fields_data_size, reader::Reader *source_reader) :
            instance_id_(instance_id),
            class_id_(class_id),
            fields_data_size_(fields_data_size),
            fields_data_(
                    reinterpret_cast<const uint8_t *>(source_reader->Extract(fields_data_size))) {}

    static std::map<const Heap *, std::map<std::string, object_id_t>> find_class_by_name_memorized;

    static std::optional<object_id_t>
    find_class_by_name_get_memorized_result(const Heap *heap, const std::string &name) {
        try {
            return find_class_by_name_memorized.at(heap).at(name);
        } catch (std::out_of_range &) {
            return std::nullopt;
        }
    }

    static void find_class_by_name_set_memorized_result(const Heap *heap, const std::string &name,
                                                        object_id_t result) {
        find_class_by_name_memorized[heap][name] = result;
    }

    static std::map<const Heap *, std::map<std::string, string_id_t>> find_string_id_memorized;

    static std::optional<string_id_t>
    find_string_id_get_memorized_result(const Heap *heap, const std::string &name) {
        try {
            return find_string_id_memorized.at(heap).at(name);
        } catch (std::out_of_range &) {
            return std::nullopt;
        }
    }

    static void find_string_id_set_memorized_result(const Heap *heap, const std::string &name,
                                                    string_id_t result) {
        find_string_id_memorized[heap][name] = result;
    }

    Heap::~Heap() {
        find_class_by_name_memorized.erase(this);
        find_string_id_memorized.erase(this);
    }


    // identifier size

    void Heap::InitializeIdSize(size_t id_size) {
        if (id_size == 0) {
            throw std::runtime_error("Invalid identifier size.");
        }
        if (id_size_ != 0) {
            throw std::runtime_error("Identifier size already initialized.");
        }
        id_size_ = id_size;
    }

    size_t Heap::GetIdSize() const {
        if (id_size_ == 0) {
            throw std::runtime_error("Identifier size is not initialized.");
        }
        return id_size_;
    }


    // class name

    void Heap::AddClassNameRecord(object_id_t class_id, string_id_t class_name_id) {
        class_names_[class_id] = class_name_id;
    }

    string_id_t Heap::GetClassNameId(object_id_t class_id) const {
        try {
            return class_names_.at(class_id);
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find class name.");
        }
    }

    std::optional<object_id_t> Heap::FindClassByName(const std::string &class_name) const {
        const std::optional<object_id_t> memorized = find_class_by_name_get_memorized_result(this,
                                                                                             class_name);
        if (memorized.has_value()) {
            if (memorized.value() == 0) {
                return std::nullopt;
            } else {
                return memorized.value();
            }
        }

        const object_id_t class_id = FindClassByNameInternal(class_name);
        find_class_by_name_set_memorized_result(this, class_name, class_id);
        if (class_id == 0) return std::nullopt;
        else return class_id;
    }

    object_id_t Heap::FindClassByNameInternal(const std::string &class_name) const {
        const string_id_t class_name_id = unwrap(FindStringId(class_name), return 0);
        const auto find_result = std::find_if(class_names_.begin(), class_names_.end(),
                                              [&](const auto &pair) {
                                                  return pair.second == class_name_id;
                                              });
        return (find_result == class_names_.end()) ? 0 : find_result->first;
    }

    std::optional<std::string> Heap::GetClassName(object_id_t class_id) const {
        try {
            return GetString(GetClassNameId(class_id));
        } catch (const std::runtime_error &) {
            return std::nullopt;
        }
    }


    // inheritance

    void Heap::AddInheritanceRecord(object_id_t class_id, object_id_t super_class_id) {
        inheritance_[class_id] = super_class_id;
    }

    std::optional<object_id_t> Heap::GetSuperClass(object_id_t class_id) const {
        try {
            const object_id_t super_class_id = inheritance_.at(class_id);
            if (super_class_id == 0) return std::nullopt;
            else return super_class_id;
        } catch (const std::out_of_range &) {
            return std::nullopt;
        }
    }

    bool Heap::ChildClassOf(object_id_t class_id, object_id_t super_class_id) const {
        object_id_t current = class_id;
        while (true) {
            if (current == super_class_id) {
                return true;
            }
            current = unwrap(GetSuperClass(current), return false);
        }
    }


    // instance field

    void Heap::AddInstanceFieldRecord(object_id_t class_id, field_t field) {
        instance_fields_[class_id].emplace_back(field);
    }

    const std::vector<field_t> &Heap::GetInstanceFields(object_id_t class_id) const {
        try {
            return instance_fields_.at(class_id);
        } catch (const std::out_of_range &) {
            return empty_fields_;
        }
    }


    // instance type

    void Heap::AddInstanceTypeRecord(object_id_t instance_id, object_type_t type) {
        instance_types_[instance_id] = type;
    }

    object_type_t Heap::GetInstanceType(object_id_t instance_id) const {
        try {
            return instance_types_.at(instance_id);
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find instance type.");
        }
    }


    // instance class

    void Heap::AddInstanceClassRecord(object_id_t instance_id, object_id_t class_id) {
        instance_classes_[instance_id] = class_id;
    }

    std::optional<object_id_t> Heap::GetClass(object_id_t instance_id) const {
        try {
            return instance_classes_.at(instance_id);
        } catch (const std::out_of_range &) {
            return std::nullopt;
        }
    }

    std::vector<object_id_t> Heap::GetInstances(object_id_t class_id) const {
        std::vector<object_id_t> result;
        for (const auto &pair: instance_classes_) {
            if (pair.second == class_id) {
                result.push_back(pair.first);
            }
        }
        return std::move(result);
    }

    bool Heap::InstanceOf(object_id_t instance_id, object_id_t class_id) const {
        object_id_t instance_class_id = unwrap(GetClass(instance_id), return false);
        return ChildClassOf(instance_class_id, class_id);
    }

    // GC root

    void Heap::MarkGcRoot(object_id_t instance_id, gc_root_type_t type) {
        gc_roots_.emplace_back(instance_id);
        gc_root_types_[instance_id] = type;
    }

    gc_root_type_t Heap::GetGcRootType(object_id_t gc_root) const {
        try {
            return gc_root_types_.at(gc_root);
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find GC root type.");
        }
    }

    const std::vector<object_id_t> &Heap::GetGcRoots() const {
        return gc_roots_;
    }


    // thread

    void Heap::AddThreadReferenceRecord(object_id_t instance_id,
                                        thread_serial_number_t thread_serial_number) {
        thread_serial_numbers_[instance_id] = thread_serial_number;
    }

    thread_serial_number_t Heap::GetThreadReference(object_id_t instance_id) const {
        try {
            return thread_serial_numbers_.at(instance_id);
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find thread serial number.");
        }
    }

    void Heap::AddThreadObjectRecord(object_id_t thread_object_id,
                                     thread_serial_number_t thread_serial_number) {
        thread_object_ids_[thread_serial_number] = thread_object_id;
    }

    object_id_t Heap::GetThreadObject(thread_serial_number_t thread_serial_number) const {
        try {
            return thread_object_ids_.at(thread_serial_number);
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find thread object.");
        }
    }

    // string

    void Heap::AddString(string_id_t string_id, const char *data, size_t length) {
        strings_[string_id] = string_t{
                .data = data,
                .length = length
        };
    }

    std::string Heap::GetString(string_id_t string_id) const {
        try {
            const string_t &string_record = strings_.at(string_id);
            return {string_record.data, string_record.length};
        } catch (const std::out_of_range &) {
            throw std::runtime_error("Failed to find string.");
        }
    }

    std::optional<string_id_t> Heap::FindStringId(const std::string &value) const {
        const std::optional<string_id_t> memorized = find_string_id_get_memorized_result(this,
                                                                                         value);
        if (memorized.has_value()) {
            if (memorized.value() == 0) {
                return std::nullopt;
            } else {
                return memorized.value();
            }
        }

        const string_id_t string_id = FindStringIdInternal(value);
        find_string_id_set_memorized_result(this, value, string_id);
        if (string_id == 0) return std::nullopt;
        else return string_id;
    }

    string_id_t Heap::FindStringIdInternal(const std::string &value) const {
        const auto find_result = std::find_if(strings_.begin(), strings_.end(),
                                              [&](const auto &pair) {
                                                  return GetString(pair.first) == value;
                                              });
        return (find_result == strings_.end()) ? 0 : find_result->first;
    }


    // fields data

    void Heap::ReadFieldsData(object_id_t instance_id, object_id_t class_id,
                              size_t fields_data_size,
                              reader::Reader *source_reader) {
        fields_data_.emplace(instance_id, HeapFieldsData(instance_id, class_id, fields_data_size,
                                                         source_reader));
    }

    const HeapFieldsData *Heap::ScopedGetFieldsData(object_id_t instance_id) const {
        try {
            return &fields_data_.at(instance_id);
        } catch (const std::out_of_range &) {
            return nullptr;
        }
    }

    std::vector<const HeapFieldsData *> Heap::ScopedGetFieldsDataList() const {
        std::vector<const HeapFieldsData *> result;
        for (const auto &[instance_id, fields_data]: fields_data_) {
            result.push_back(&fields_data);
        }
        return result;
    }


    // primitive

    void Heap::ReadPrimitive(object_id_t referrer_id, string_id_t field_name_id, value_type_t type,
                             reader::Reader *source_reader) {
        primitives_[referrer_id].emplace(field_name_id, HeapPrimitiveData(type, source_reader));
    }

    void Heap::ReadPrimitiveArray(object_id_t primitive_array_object_id, value_type_t type,
                                  size_t data_size,
                                  reader::Reader *source_reader) {
        primitive_arrays_.emplace(primitive_array_object_id,
                                  HeapPrimitiveArrayData(type, data_size, source_reader));
    }

    const HeapPrimitiveData *
    Heap::ScopedGetPrimitiveData(object_id_t instance_id, string_id_t field_name_id) const {
        try {
            return &primitives_.at(instance_id).at(field_name_id);
        } catch (const std::out_of_range &) {
            return nullptr;
        }
    }

    const HeapPrimitiveArrayData *
    Heap::ScopedGetPrimitiveArrayData(object_id_t primitive_array_object_id) const {
        try {
            return &primitive_arrays_.at(primitive_array_object_id);
        } catch (const std::out_of_range &) {
            return nullptr;
        }
    }

    std::optional<uint64_t>
    Heap::GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const {
        const string_id_t reference_name_id = unwrap(FindStringId(reference_name),
                                                     return std::nullopt);
        const heap::HeapPrimitiveData *data = ScopedGetPrimitiveData(referrer_id,
                                                                     reference_name_id);
        if (data == nullptr) return std::nullopt;
        return data->GetValue<uint64_t>();
    }

    std::optional<std::vector<uint64_t>>
    Heap::GetArrayPrimitiveRaw(object_id_t primitive_array_id) const {
        const heap::HeapPrimitiveArrayData *data_list = ScopedGetPrimitiveArrayData(
                primitive_array_id);
        if (data_list == nullptr) return std::nullopt;
        std::vector<uint64_t> result;

        reader::Reader reader(data_list->GetData(), data_list->GetSize());
        for (size_t i = 0; i < data_list->GetLength(); ++i) {
            HeapPrimitiveData data(data_list->GetType(), &reader);
            result.emplace_back(data.GetValue<uint64_t>());
        }
        return result;
    }


    // reference

    void Heap::AddFieldReference(object_id_t referrer_id, string_id_t field_name_id,
                                 object_id_t referent_id,
                                 bool static_reference) {
        AddReference(referrer_id,
                     reference_t{
                             .type = static_reference ? kStaticField : kInstanceField,
                             .field_name_id = field_name_id},
                     referent_id);
    }

    void Heap::AddFieldExcludedReference(object_id_t referrer_id, string_id_t field_name_id,
                                         object_id_t referent_id,
                                         bool static_reference) {
        AddExcludedReference(referrer_id,
                             reference_t{
                                     .type = static_reference ? kStaticField : kInstanceField,
                                     .field_name_id = field_name_id},
                             referent_id);
    }

    std::optional<object_id_t>
    Heap::GetFieldReference(object_id_t referrer_id, const std::string &reference_name,
                            bool with_excluded) const {
        const string_id_t reference_name_id = unwrap(FindStringId(reference_name),
                                                     return std::nullopt);
        try {
            for (const auto &pair: references_.at(referrer_id)) {
                if (pair.second.field_name_id == reference_name_id) {
                    return pair.first;
                }
            }
        } catch (const std::out_of_range &) {}

        try {
            if (with_excluded) {
                for (const auto &pair: excluded_references_.at(referrer_id)) {
                    if (pair.second.field_name_id == reference_name_id) {
                        return pair.first;
                    }
                }
            }
        } catch (const std::out_of_range &) {}

        return std::nullopt;
    }

    void Heap::AddArrayReference(object_id_t referrer_id, size_t index, object_id_t referent_id) {
        AddReference(referrer_id, reference_t{.type = kArrayElement, .index = index}, referent_id);
    }

    std::optional<object_id_t>
    Heap::GetArrayReference(object_id_t referrer_id, size_t index) const {
        try {
            for (const auto &pair: references_.at(referrer_id)) {
                if (pair.second.index == index) {
                    return pair.first;
                }
            }
            return std::nullopt;
        } catch (const std::out_of_range &) {
            return std::nullopt;
        }
    }

    void Heap::ExcludeReferences(object_id_t referrer_id) {
        try {
            for (const auto &pair: references_.at(referrer_id)) {
                AddExcludedReference(referrer_id, pair.second, pair.first);
            }
            references_.erase(referrer_id);
        } catch (const std::out_of_range &) {}
    }

    const std::map<object_id_t, std::map<object_id_t, reference_t>> &
    Heap::GetLeakReferenceGraph() const {
        return references_;
    }

    void
    Heap::AddReference(object_id_t referrer_id, reference_t reference, object_id_t referent_id) {
        references_[referrer_id][referent_id] = reference;
    }

    void Heap::AddExcludedReference(object_id_t referrer_id, reference_t reference,
                                    object_id_t referent_id) {
        excluded_references_[referrer_id][referent_id] = reference;
    }


    // tools

    static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;

    /**
     * Content of JVM string is encoded in UTF-16 that is two bytes per character. The function converts it to an
     * std::string instance.
     */
    static std::string convert_utf_16_string(const uint8_t *buffer, size_t buffer_size) {
        std::vector<char16_t> temp;
        reader::Reader reader(buffer, buffer_size);
        const size_t length = buffer_size / sizeof(char16_t);
        for (size_t i = 0; i < length; ++i) {
            temp.emplace_back(reader.Read(sizeof(char16_t)));
        }
        return converter.to_bytes(std::u16string(temp.data(), temp.size()));
    }

    std::optional<std::string>
    Heap::GetValueFromStringInstance(object_id_t instance_id) const {
        const object_id_t string_class_id = unwrap(FindClassByName("java.lang.String"),
                                                   return std::nullopt);
        if (!InstanceOf(instance_id, string_class_id)) return std::nullopt;

        const object_id_t string_content_array = unwrap(GetFieldReference(instance_id, "value"),
                                                        return std::nullopt);
        const heap::HeapPrimitiveArrayData *string_content_array_data =
                ScopedGetPrimitiveArrayData(string_content_array);
        if (string_content_array_data == nullptr) return std::nullopt;
        if (string_content_array_data->GetType() == heap::value_type_t::kChar) {
            const size_t offset =
                    GetFieldPrimitive<int32_t>(instance_id, "offset").value_or(0) *
                    heap::get_value_type_size(heap::value_type_t::kChar);
            const size_t size =
                    std::min(
                            GetFieldPrimitive<int32_t>(instance_id, "count").value_or(0) *
                            heap::get_value_type_size(heap::value_type_t::kChar),
                            string_content_array_data->GetSize() - offset
                    );
            return convert_utf_16_string(string_content_array_data->GetData() + offset, size);
        } else if (string_content_array_data->GetType() == heap::value_type_t::kByte) {
            return std::string(
                    reinterpret_cast<const char *>(string_content_array_data->GetData()),
                    string_content_array_data->GetSize());
        } else {
            throw std::runtime_error("Unexpected array type for field java.lang.String.value.");
        }
    }

    const std::vector<field_t> Heap::empty_fields_ = {};
}