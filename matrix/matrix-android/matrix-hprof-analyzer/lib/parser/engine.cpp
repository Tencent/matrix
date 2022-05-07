#include "internal/engine.h"
#include "errorha.h"

#include "macro.h"

#include <sstream>

namespace matrix::hprof::internal::parser {

    namespace tag {
        static constexpr uint8_t kStrings = 0x01;
        static constexpr uint8_t kLoadClasses = 0x02;
        static constexpr uint8_t kHeapDump = 0x0c;
        static constexpr uint8_t kHeapDumpSegment = 0x1c;
        static constexpr uint8_t kHeapDumpEnd = 0x2c;

        /**
         * Traditional heap tag.
         */

        static constexpr uint8_t kHeapRootUnknown = 0xff;
        static constexpr uint8_t kHeapRootJniGlobal = 0x01;
        static constexpr uint8_t kHeapRootJniLocal = 0x02;
        static constexpr uint8_t kHeapRootJavaFrame = 0x03;
        static constexpr uint8_t kHeapRootNativeStack = 0x04;
        static constexpr uint8_t kHeapRootStickyClass = 0x05;
        static constexpr uint8_t kHeapRootThreadBlock = 0x06;
        static constexpr uint8_t kHeapRootMonitorUsed = 0x07;
        static constexpr uint8_t kHeapRootThreadObject = 0x08;

        static constexpr uint8_t kHeapClassDump = 0x20;
        static constexpr uint8_t kHeapInstanceDump = 0x21;
        static constexpr uint8_t kHeapObjectArrayDump = 0x22;
        static constexpr uint8_t kHeapPrimitiveArrayDump = 0x23;

        /**
         * Android additional heap tag.
         */

        static constexpr uint8_t kHeapRootInternedString = 0x89;
        static constexpr uint8_t kHeapRootFinalizing = 0x8a;
        static constexpr uint8_t kHeapRootDebugger = 0x8b;
        static constexpr uint8_t kHeapRootReferenceCleanup = 0x8c;
        static constexpr uint8_t kHeapRootVMInternal = 0x8d;
        static constexpr uint8_t kHeapRootJniMonitor = 0x8e;
        static constexpr uint8_t kHeapRootUnreachable = 0x90;

        static constexpr uint8_t kHeapPrimitiveArrayNoDataDump = 0xc3;
        static constexpr uint8_t kHeapDumpInfo = 0xfe;
    }

    void
    HeapParserEngineImpl::Parse(reader::Reader &reader, heap::Heap &heap,
                                const ExcludeMatcherGroup &exclude_matcher_group,
                                const HeapParserEngine &next) const {
        next.ParseHeader(reader, heap);

        while (true) {
            const uint8_t tag = reader.ReadU1();
            reader.SkipU4(); // Skip timestamp.
            const uint32_t length = reader.ReadU4();
            switch (tag) {
                case tag::kStrings:
                    next.ParseStringRecord(reader, heap, length);
                    break;
                case tag::kLoadClasses:
                    next.ParseLoadClassRecord(reader, heap, length);
                    break;
                case tag::kHeapDump:
                case tag::kHeapDumpSegment:
                    next.ParseHeapContent(reader, heap, length, exclude_matcher_group, next);
                    break;
                case tag::kHeapDumpEnd:
                    goto break_read_loop;
                default:
                    reader.Skip(length);
                    break;
            }
        }
        break_read_loop:
        next.LazyParse(heap, exclude_matcher_group);
    }

    void HeapParserEngineImpl::ParseHeader(reader::Reader &reader, heap::Heap &heap) const {
        // Version string.
        const std::string version = reader.ReadNullTerminatedString();
        if (version != "JAVA PROFILE 1.0" &&
            version != "JAVA PROFILE 1.0.1" &&
            version != "JAVA PROFILE 1.0.2" &&
            version != "JAVA PROFILE 1.0.3") {
            pub_fatal("invalid HPROF header");
        }
        // Identifier size.
        heap.InitializeIdSize(reader.ReadU4());
        // Skip timestamp.
        reader.SkipU8();
    }

    void HeapParserEngineImpl::ParseStringRecord(reader::Reader &reader, heap::Heap &heap,
                                                 size_t record_length) const {
        const heap::string_id_t string_id = reader.Read(heap.GetIdSize());
        const size_t length = record_length - heap.GetIdSize();
        heap.AddString(string_id, reinterpret_cast<const char *>(reader.Extract(length)), length);
    }

    void
    HeapParserEngineImpl::ParseLoadClassRecord(reader::Reader &reader, heap::Heap &heap,
                                               size_t record_length) const {
        // Skip class serial number.
        reader.SkipU4();
        // Class object ID.
        const heap::object_id_t class_id = reader.Read(heap.GetIdSize());
        // Skip stack trace serial number.
        reader.SkipU4();
        // Class name string ID.
        const heap::string_id_t class_name_id = reader.Read(heap.GetIdSize());
        heap.AddClassNameRecord(class_id, class_name_id);
    }

    static std::optional<std::string>
    get_thread_name(const heap::Heap &heap, heap::object_id_t thread_object_id) {
        const heap::object_id_t thread_class_id =
                unwrap(heap.FindClassByName("java.lang.Thread"), return std::nullopt);
        if (!heap.InstanceOf(thread_object_id, thread_class_id)) return std::nullopt;
        const heap::object_id_t name_string_id =
                unwrap(heap.GetFieldReference(thread_object_id, "name"), return std::nullopt);
        return heap.GetValueFromStringInstance(name_string_id);
    }

    void HeapParserEngineImpl::ParseHeapContent(reader::Reader &reader, heap::Heap &heap,
                                                size_t record_length,
                                                const ExcludeMatcherGroup &exclude_matcher_group,
                                                const HeapParserEngine &next) const {
        size_t bytes_read = 0;
        while (bytes_read < record_length) {
            const uint8_t tag = reader.ReadU1();
            bytes_read += sizeof(uint8_t);
            switch (tag) {
                case tag::kHeapRootUnknown:
                    bytes_read += next.ParseHeapContentRootUnknownSubRecord(reader, heap);
                    break;
                case tag::kHeapRootJniGlobal:
                    bytes_read += next.ParseHeapContentRootJniGlobalSubRecord(reader, heap);
                    break;
                case tag::kHeapRootJniLocal:
                    bytes_read += next.ParseHeapContentRootJniLocalSubRecord(reader, heap);
                    break;
                case tag::kHeapRootJavaFrame:
                    bytes_read += next.ParseHeapContentRootJavaFrameSubRecord(reader, heap);
                    break;
                case tag::kHeapRootNativeStack:
                    bytes_read += next.ParseHeapContentRootNativeStackSubRecord(reader, heap);
                    break;
                case tag::kHeapRootStickyClass:
                    bytes_read += next.ParseHeapContentRootStickyClassSubRecord(reader, heap);
                    break;
                case tag::kHeapRootThreadBlock:
                    bytes_read += next.ParseHeapContentRootThreadBlockSubRecord(reader, heap);
                    break;
                case tag::kHeapRootMonitorUsed:
                    bytes_read += next.ParseHeapContentRootMonitorUsedSubRecord(reader, heap);
                    break;
                case tag::kHeapRootThreadObject:
                    bytes_read += next.ParseHeapContentRootThreadObjectSubRecord(reader, heap);
                    break;
                case tag::kHeapRootInternedString:
                    bytes_read += next.ParseHeapContentRootInternedStringSubRecord(reader, heap);
                    break;
                case tag::kHeapRootFinalizing:
                    bytes_read += next.ParseHeapContentRootFinalizingSubRecord(reader, heap);
                    break;
                case tag::kHeapRootDebugger:
                    bytes_read += next.ParseHeapContentRootDebuggerSubRecord(reader, heap);
                    break;
                case tag::kHeapRootReferenceCleanup:
                    bytes_read += next.ParseHeapContentRootReferenceCleanupSubRecord(reader, heap);
                    break;
                case tag::kHeapRootVMInternal:
                    bytes_read += next.ParseHeapContentRootVMInternalSubRecord(reader, heap);
                    break;
                case tag::kHeapRootJniMonitor:
                    bytes_read += next.ParseHeapContentRootJniMonitorSubRecord(reader, heap);
                    break;
                case tag::kHeapRootUnreachable:
                    bytes_read += next.ParseHeapContentRootUnreachableSubRecord(reader, heap);
                    break;
                case tag::kHeapClassDump:
                    bytes_read += next.ParseHeapContentClassSubRecord(reader, heap,
                                                                      exclude_matcher_group);
                    break;
                case tag::kHeapInstanceDump:
                    bytes_read += next.ParseHeapContentInstanceSubRecord(reader, heap);
                    break;
                case tag::kHeapObjectArrayDump:
                    bytes_read += next.ParseHeapContentObjectArraySubRecord(reader, heap);
                    break;
                case tag::kHeapPrimitiveArrayDump:
                    bytes_read += next.ParseHeapContentPrimitiveArraySubRecord(reader, heap);
                    break;
                case tag::kHeapPrimitiveArrayNoDataDump:
                    bytes_read += next.ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader,
                                                                                         heap);
                    break;
                case tag::kHeapDumpInfo:
                    bytes_read += next.SkipHeapContentInfoSubRecord(reader, heap);
                    break;
                default:
                    std::stringstream error_builder;
                    error_builder << "unsupported heap dump tag " << std::to_string(tag);
                    pub_fatal(error_builder.str());
            }
        }
    }

    struct field_exclude_matcher_flatten_t {
        const heap::object_id_t class_id;
        const heap::string_id_t field_name_id;
    };

    static void
    flatten_field_exclude_matchers(std::vector<field_exclude_matcher_flatten_t> &flatten_matchers,
                                   const heap::Heap &heap,
                                   const std::vector<FieldExcludeMatcher> &matchers) {
        for (const auto &matcher: matchers) {
            flatten_matchers.emplace_back(field_exclude_matcher_flatten_t{
                    .class_id = matcher.FullMatchClassName()
                                ? 0
                                : unwrap(heap.FindClassByName(matcher.GetClassName()), continue),
                    .field_name_id = matcher.FullMatchFieldName()
                                     ? 0
                                     : unwrap(heap.FindStringId(matcher.GetFieldName()), continue)
            });
        }
    }

    struct native_global_exclude_matcher_flatten_t {
        const heap::string_id_t class_id;
    };

    static void
    flatten_native_global_exclude_matchers(
            std::vector<native_global_exclude_matcher_flatten_t> &flatten_matchers,
            const heap::Heap &heap,
            const std::vector<NativeGlobalExcludeMatcher> &matchers) {
        for (const auto &matcher: matchers) {
            flatten_matchers.emplace_back(native_global_exclude_matcher_flatten_t{
                    .class_id = matcher.FullMatchClassName()
                                ? 0
                                : unwrap(heap.FindClassByName(matcher.GetClassName()), continue)
            });
        }
    }

    void HeapParserEngineImpl::LazyParse(heap::Heap &heap,
                                         const ExcludeMatcherGroup &exclude_matcher_group) const {
        std::vector<field_exclude_matcher_flatten_t> instance_field_matchers;
        flatten_field_exclude_matchers(instance_field_matchers, heap,
                                       exclude_matcher_group.instance_fields_);

        // Analyze instances.
        for (const auto task: heap.ScopedGetFieldsDataList()) {
            reader::Reader fields_data_reader = task->GetScopeReader();

            const heap::object_id_t referrer_id = task->GetInstanceId();
            const heap::object_id_t referrer_class_id = task->GetClassId();

            heap::object_id_t current_class_id = referrer_class_id;
            std::vector<field_exclude_matcher_flatten_t> class_match_matchers;
            for (const auto &matcher: instance_field_matchers) {
                if (matcher.class_id == 0 ||
                    heap.ChildClassOf(referrer_class_id, matcher.class_id)) {
                    class_match_matchers.emplace_back(matcher);
                }
            }

            while (true) {
                for (const auto &field: heap.GetInstanceFields(current_class_id)) {
                    if (field.type == heap::value_type_t::kObject) {
                        heap::object_id_t referent_id = fields_data_reader.Read(heap.GetIdSize());
                        { // Add reference.
                            bool exclude = false;
                            // Find exclude matcher of instance field.
                            for (const auto &matcher: class_match_matchers) {
                                if (matcher.field_name_id == 0 ||
                                    matcher.field_name_id == field.name_id) {
                                    exclude = true;
                                    break;
                                }
                            }
                            if (exclude)
                                heap.AddFieldExcludedReference(referrer_id, field.name_id,
                                                               referent_id);
                            else heap.AddFieldReference(referrer_id, field.name_id, referent_id);
                        }
                    } else {
                        heap.ReadPrimitive(referrer_id, field.name_id, field.type,
                                           &fields_data_reader);
                    }
                }
                current_class_id = unwrap(heap.GetSuperClass(current_class_id), break);
            }
        }

        // Remove exclude GC roots references.
        std::vector<native_global_exclude_matcher_flatten_t> native_global_matchers;
        flatten_native_global_exclude_matchers(native_global_matchers, heap,
                                               exclude_matcher_group.native_globals_);
        for (const auto gc_root: heap.GetGcRoots()) {
            if (heap.GetGcRootType(gc_root) == heap::gc_root_type_t::kRootJavaFrame) {
                const heap::object_id_t thread_object_id = heap.GetThreadObject(
                        heap.GetThreadReference(gc_root));
                const std::string thread_name = unwrap(get_thread_name(heap, thread_object_id),
                                                       continue);
                for (const auto &matcher: exclude_matcher_group.threads_) {
                    if (matcher.FullMatchThreadName() || matcher.GetThreadName() == thread_name) {
                        heap.ExcludeReferences(gc_root);
                        break;
                    }
                }
            }

            // Find exclude matcher of native global reference.
            if (heap.GetGcRootType(gc_root) == heap::gc_root_type_t::kRootJniGlobal) {
                for (const auto &matcher: native_global_matchers) {
                    if (matcher.class_id == 0 || heap.InstanceOf(gc_root, matcher.class_id)) {
                        heap.ExcludeReferences(gc_root);
                    }
                }
            }
        }
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootUnknownSubRecord(reader::Reader &reader,
                                                               heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootUnknown);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootJniGlobalSubRecord(reader::Reader &reader,
                                                                 heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootJniGlobal);
        reader.Skip(heap.GetIdSize()); // Skip JNI global reference ID.
        return heap.GetIdSize() + heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootJniLocalSubRecord(reader::Reader &reader,
                                                                heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootJniLocal);
        reader.SkipU4(); // Skip thread serial number.
        reader.SkipU4(); // Skip frame number.
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootJavaFrameSubRecord(reader::Reader &reader,
                                                                 heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootJavaFrame);
        const heap::thread_serial_number_t thread_serial_number = reader.ReadU4();
        if (object_id != 0) {
            heap.AddThreadReferenceRecord(object_id, thread_serial_number);
        }
        reader.SkipU4(); // Skip frame number.
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootNativeStackSubRecord(reader::Reader &reader,
                                                                   heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootNativeStack);
        reader.SkipU4(); // Skip thread serial number.
        return heap.GetIdSize() + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootStickyClassSubRecord(reader::Reader &reader,
                                                                   heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootStickyClass);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootThreadBlockSubRecord(reader::Reader &reader,
                                                                   heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootThreadBlock);
        reader.SkipU4(); // Skip thread serial number.
        return heap.GetIdSize() + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootMonitorUsedSubRecord(reader::Reader &reader,
                                                                   heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootMonitorUsed);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootThreadObjectSubRecord(reader::Reader &reader,
                                                                    heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootThreadObject);
        const heap::thread_serial_number_t thread_serial_number = reader.ReadU4();
        if (object_id != 0) {
            heap.AddThreadObjectRecord(object_id, thread_serial_number);
        }
        reader.SkipU4(); // Skip stack trace serial number.
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootInternedStringSubRecord(reader::Reader &reader,
                                                                      heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootInternedString);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootFinalizingSubRecord(reader::Reader &reader,
                                                                  heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootFinalizing);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootDebuggerSubRecord(reader::Reader &reader,
                                                                heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootDebugger);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootReferenceCleanupSubRecord(reader::Reader &reader,
                                                                        heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootReferenceCleanup);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootVMInternalSubRecord(reader::Reader &reader,
                                                                  heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootVMInternal);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootJniMonitorSubRecord(reader::Reader &reader,
                                                                  heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootJniMonitor);
        reader.SkipU4(); // Skip stack trace serial number.
        reader.SkipU4(); // Skip stack depth.
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t);
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentRootUnreachableSubRecord(reader::Reader &reader,
                                                                   heap::Heap &heap) const {
        const heap::object_id_t object_id = reader.Read(heap.GetIdSize());
        if (object_id != 0) heap.MarkGcRoot(object_id, heap::gc_root_type_t::kRootUnreachable);
        return heap.GetIdSize();
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentClassSubRecord(reader::Reader &reader, heap::Heap &heap,
                                                         const ExcludeMatcherGroup &exclude_matcher_group) const {
        const heap::object_id_t class_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceTypeRecord(class_id, heap::object_type_t::kClass);

        reader.SkipU4(); // Skip stack trace serial number.

        const heap::object_id_t super_class_id = reader.Read(heap.GetIdSize());
        heap.AddInheritanceRecord(class_id, super_class_id);

        reader.Skip(heap.GetIdSize()); // Skip class loader object ID.
        reader.Skip(heap.GetIdSize()); // Skip signers object ID.
        reader.Skip(heap.GetIdSize()); // Skip protection domain object ID.
        reader.Skip(heap.GetIdSize()); // Skip reserved space.
        reader.Skip(heap.GetIdSize()); // Skip reserved space.
        reader.SkipU4(); // Skip instance size.

        size_t content_size = 0;

        // Skip constant pool.
        const size_t constant_pool_length = reader.ReadU2();
        content_size += sizeof(uint16_t);
        for (int i = 0; i < constant_pool_length; ++i) {
            reader.SkipU2(); // Skip constant pool element index.
            content_size += sizeof(uint16_t);
            const size_t type_size = ({
                const heap::value_type_t type = heap::value_type_cast(reader.ReadU1());
                content_size += sizeof(uint8_t);
                (type == heap::value_type_t::kObject) ? heap.GetIdSize()
                                                      : heap::get_value_type_size(type);
            });
            reader.Skip(type_size);
            content_size += type_size;
        }

        // Read static fields.
        const size_t static_fields_length = reader.ReadU2();
        content_size += sizeof(uint16_t);

        std::vector<field_exclude_matcher_flatten_t> static_exclude_matchers;
        flatten_field_exclude_matchers(static_exclude_matchers, heap,
                                       exclude_matcher_group.static_fields_);
        std::vector<field_exclude_matcher_flatten_t> class_match_matchers;
        for (const auto &matcher: static_exclude_matchers) {
            if (matcher.class_id == 0 || heap.ChildClassOf(class_id, matcher.class_id)) {
                class_match_matchers.emplace_back(matcher);
            }
        }

        for (int i = 0; i < static_fields_length; ++i) {
            const heap::string_id_t field_name_id = reader.Read(heap.GetIdSize());
            content_size += heap.GetIdSize();
            const auto type = heap::value_type_cast(reader.ReadU1());
            content_size += sizeof(uint8_t);
            if (type == heap::value_type_t::kObject) {
                const heap::object_id_t referent_id = reader.Read(heap.GetIdSize());
                content_size += heap.GetIdSize();
                { // Add reference.
                    bool exclude = false;
                    // Find exclude matcher of static field.
                    for (const auto &matcher: class_match_matchers) {
                        if (matcher.field_name_id == 0 || matcher.field_name_id == field_name_id) {
                            exclude = true;
                            break;
                        }
                    }
                    if (exclude)
                        heap.AddFieldExcludedReference(class_id, field_name_id, referent_id, true);
                    else heap.AddFieldReference(class_id, field_name_id, referent_id, true);
                }
            } else {
                const size_t type_size = heap::get_value_type_size(type);
                reader.Skip(type_size);
                content_size += type_size;
            }
        }

        // Read instance fields.
        const size_t non_static_fields_length = reader.ReadU2();
        content_size += sizeof(uint16_t);
        for (int i = 0; i < non_static_fields_length; ++i) {
            const heap::string_id_t field_name_id = reader.Read(heap.GetIdSize());
            content_size += heap.GetIdSize();
            const auto type = heap::value_type_cast(reader.ReadU1());
            content_size += sizeof(uint8_t);
            heap.AddInstanceFieldRecord(class_id,
                                        heap::field_t{.name_id = field_name_id, .type = type});
        }

        return heap.GetIdSize() + sizeof(uint32_t) + (heap.GetIdSize() * 6) + sizeof(uint32_t) +
               content_size;
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentInstanceSubRecord(reader::Reader &reader,
                                                            heap::Heap &heap) const {
        const heap::object_id_t instance_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceTypeRecord(instance_id, heap::object_type_t::kInstance);

        reader.SkipU4(); // Skip stack trace serial number.

        const heap::object_id_t class_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceClassRecord(instance_id, class_id);

        const size_t fields_data_size = reader.ReadU4();
        heap.ReadFieldsData(instance_id, class_id, fields_data_size, &reader);

        return heap.GetIdSize() + sizeof(uint32_t) + heap.GetIdSize() + sizeof(uint32_t) +
               fields_data_size;
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentObjectArraySubRecord(reader::Reader &reader,
                                                               heap::Heap &heap) const {
        const heap::object_id_t array_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceTypeRecord(array_id, heap::object_type_t::kObjectArray);

        reader.SkipU4(); // Skip stack trace serial number.
        const size_t array_length = reader.ReadU4();
        const heap::object_id_t class_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceClassRecord(array_id, class_id);
        for (size_t i = 0; i < array_length; ++i) {
            const heap::object_id_t referent_id = reader.Read(heap.GetIdSize());
            heap.AddArrayReference(array_id, i, referent_id);
        }
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t) + heap.GetIdSize() +
               (array_length * heap.GetIdSize());
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentPrimitiveArraySubRecord(reader::Reader &reader,
                                                                  heap::Heap &heap) const {
        const heap::object_id_t array_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceTypeRecord(array_id, heap::object_type_t::kPrimitiveArray);

        reader.SkipU4(); // Skip stack trace serial number.
        const size_t array_length = reader.ReadU4();
        const heap::value_type_t type = heap::value_type_cast(reader.ReadU1());
        const size_t array_size = array_length * heap::get_value_type_size(type);
        heap.ReadPrimitiveArray(array_id, type, array_size, &reader);
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t) +
               array_size;
    }

    size_t
    HeapParserEngineImpl::ParseHeapContentPrimitiveArrayNoDataDumpSubRecord(reader::Reader &reader,
                                                                            heap::Heap &heap) const {
        const heap::object_id_t array_id = reader.Read(heap.GetIdSize());
        heap.AddInstanceTypeRecord(array_id, heap::object_type_t::kPrimitiveArray);

        reader.SkipU4(); // Skip stack trace serial number.
        reader.SkipU4(); // Skip array length;
        reader.SkipU1(); // Skip array type.
        return heap.GetIdSize() + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint8_t);
    }

    size_t
    HeapParserEngineImpl::SkipHeapContentInfoSubRecord(reader::Reader &reader,
                                                       const heap::Heap &heap) const {
        reader.SkipU4();
        reader.Skip(heap.GetIdSize());
        return sizeof(uint32_t) + heap.GetIdSize();
    }
}