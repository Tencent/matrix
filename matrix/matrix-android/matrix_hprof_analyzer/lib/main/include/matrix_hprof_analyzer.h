#ifndef __matrix_hprof_analyzer_h__
#define __matrix_hprof_analyzer_h__

#include <functional>
#include <string>
#include <optional>
#include <vector>

#include "macro.h"

namespace matrix::hprof {

    typedef uint64_t object_id_t;

    class HprofHeapImpl;

    /**
     * The class which instances are used obtain relations between objects in the heap.
     */
    class HprofHeap {
    public:
        /**
         * Gets class identifier in HPROF file with corresponding class name \a class_name, or returns std::nullopt if
         * the record is not found.
         */
        [[nodiscard]] std::optional<object_id_t>
        FindClassByName(const std::string &class_name) const;

        /**
         * Gets class name with class identifier in HPROF file \a class_id, or returns std::nullopt if the record is not
         * found.
         */
        [[nodiscard]] std::optional<std::string>
        GetClassName(object_id_t class_id) const;

        /**
         * Gets super class identifier in HPROF file of class which identifier is \a class_id, or returns std::nullopt
         * if the record is not found.
         */
        [[nodiscard]] std::optional<object_id_t>
        GetSuperClass(object_id_t class_id) const;

        /**
         * Checks whether the class with identifier \a class_id is a child class of class with identifier
         * \a super_class_id.
         */
        [[nodiscard]] bool ChildClassOf(object_id_t class_id, object_id_t super_class_id) const;

        /**
         * Gets class identifier in HPROF file which the object with \a instance_id is its instance, or returns
         * std::nullopt if the record is not found.
         */
        [[nodiscard]] std::optional<object_id_t>
        GetClass(object_id_t instance_id) const;

        /**
         * Gets all instance identifiers in HPROF file of class with id \a class_id.
         */
        [[nodiscard]] std::vector<object_id_t>
        GetInstances(object_id_t class_id) const;

        /**
         * Checks whether the object with identifier \a instance_id is the instance of class with identifier
         * \a class_id.
         */
        [[nodiscard]] bool InstanceOf(object_id_t instance_id, object_id_t class_id) const;


    public:
        /**
         * Gets object identifier in HPROF file which is referred by the field named \a field_name of the object with
         * identifier \a referrer_id, or returns std::nullopt if the record is not found.
         */
        [[nodiscard]] std::optional<object_id_t>
        GetFieldReference(object_id_t referrer_id, const std::string &field_name) const;

        /**
         * Gets object identifier in HPROF file which is referred as an array element indexed \a index of the array with
         * identifier \a referrer_id, or returns std::nullopt if the record is not found.
         */
        [[nodiscard]] std::optional<object_id_t>
        GetArrayReference(object_id_t referrer_id, size_t index) const;

        /**
         * Gets primitive value of the field named \a field_name of the object with identifier \a referrer_id and
         * converts as type \a T, or returns std::nullopt if the record is not found.
         */
        template<typename T>
        [[nodiscard]] std::optional<T>
        GetFieldPrimitive(object_id_t referrer_id, const std::string &reference_name) const {
            const uint64_t data = unwrap(GetFieldPrimitiveRaw(referrer_id, reference_name), return std::nullopt);
            return *reinterpret_cast<const T *>(&data);
        }

    private:
        [[nodiscard]] std::optional<uint64_t>
        GetFieldPrimitiveRaw(object_id_t referrer_id, const std::string &reference_name) const;


    public:
        /**
         * Gets primitive value which as an array element indexed \a index of the array with identifier \a referrer_id,
         * or returns std::nullopt if the record is not found.
         */
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

    private:
        [[nodiscard]] std::optional<std::vector<uint64_t>>
        GetArrayPrimitiveRaw(object_id_t primitive_array_id) const;


    public:
        /**
         * Gets string content from java.lang.String object with identifier \a instance_id, or returns std::nullopt if
         * the record is not found or the object is not a java.lang.String instance.
         */
        [[nodiscard]] std::optional<std::string>
        GetValueFromStringInstance(object_id_t instance_id) const;

    private:
        friend class HprofAnalyzerImpl;

        explicit HprofHeap(HprofHeapImpl *impl);

        friend_test(main_heap, delegate);

        std::unique_ptr<HprofHeapImpl> impl_;
    };

    /**
     * Shortest path from GC root to the leaking object.
     */
    class LeakChain {
    public:
        /**
         * The nearest GC root to the chain leaking object.
         */
        class GcRoot {
        public:
            /**
             * GC root type.
             */
            enum class Type {
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
                kRootUnreachable,
            };

            /**
             * Gets GC root class name.
             * <p>
             * The function returns class name of the GC root object, or self class name of root if GC root is an class
             * and refer next node as static field.
             */
            [[nodiscard]] const std::string &GetName() const;

            /**
             * Gets GC root type.
             */
            [[nodiscard]] Type GetType() const;

        private:
            friend GcRoot create_leak_chain_gc_root(const std::string &name, Type type);

            GcRoot(std::string name, Type type);

            std::string name_;
            Type type_;
        };

        /**
         * Node of the leaking chain.
         */
        class Node {
        public:
            /**
             * Reference type.
             */
            enum class ReferenceType {
                kStaticField,
                kInstanceField,
                kArrayElement
            };

            /**
             * Referent object type.
             */
            enum class ObjectType {
                kClass,
                kInstance,
                kObjectArray,
                kPrimitiveArray
            };

            /**
             * Gets name of reference which the previous node used to referring current node object.
             * <p>
             * The function returns field name if the previous node is referring current node as instance or static
             * field, or array index if current node is an element of previous node.
             */
            [[nodiscard]] const std::string &GetReference() const;

            /**
             * Get referent type.
             */
            [[nodiscard]] ReferenceType GetReferenceType() const;

            /**
             * Gets node object class name.
             * <p>
             * The function returns class name of the node object, or self class name of root if node object is an class
             * and refer next node as static field.
             */
            [[nodiscard]] const std::string &GetObject() const;

            /**
            * Gets node object type.
            */
            [[nodiscard]] ObjectType GetObjectType() const;

        private:
            friend Node create_leak_chain_node(const std::string &reference, Node::ReferenceType reference_type,
                                               const std::string &object, Node::ObjectType object_type);

            Node(std::string reference, Node::ReferenceType reference_type,
                 std::string object, Node::ObjectType object_type);

            std::string reference_;
            ReferenceType reference_type_;
            std::string object_;
            ObjectType object_type_;
        };

        /**
         * Gets GC root info of leak chain.
         */
        [[nodiscard]] const GcRoot &GetGcRoot() const;

        /**
         * Gets chain nodes of leak chain. The last node of the returned vector is the leaking object, or function
         * returns empty vector if GC root is the tracked "leaking" object.
         */
        [[nodiscard]] const std::vector<Node> &GetNodes() const;

        /**
         * Gets number of hops from GC root to the leaking object.
         */
        [[nodiscard]] size_t GetDepth() const;

    private:
        friend LeakChain create_leak_chain(const GcRoot &gc_root, const std::vector<Node> &nodes);

        LeakChain(GcRoot gc_root, std::vector<Node> nodes);

        GcRoot gc_root_;
        std::vector<Node> nodes_;
        size_t depth_;
    };

    class HprofAnalyzerImpl;

    /**
     * HPROF file analyzer.
     * <p>
     * The class used to find shortest leaking path of leaking objects.
     */
    class HprofAnalyzer {
    public:
        /**
         * Create analyzer for HPROF file with file descriptor \a hprof_fd.
         */
        explicit HprofAnalyzer(int hprof_fd);

        ~HprofAnalyzer();

        /**
         * Exclude references referred as instance field \a field_name of class \a class_name instance.
         * <p>
         * Use "*" to match all.
         */
        void ExcludeInstanceFieldReference(const std::string &class_name, const std::string &field_name);

        /**
         * Exclude references referred as static field \a field_name of class \a class_name.
         * <p>
         * Use "*" to match all.
         */
        void ExcludeStaticFieldReference(const std::string &class_name, const std::string &field_name);

        /**
         * Exclude references referred from stack frame of thread \a thread_name.
         * <p>
         * Use "*" to match all.
         */
        void ExcludeThreadReference(const std::string &thread_name);

        /**
         * Exclude native global references.
         * <p>
         * Use "*" to match all.
         */
        void ExcludeNativeGlobalReference(const std::string &class_name);

        /**
         * Start analyzing the HPROF file.
         * <p>
         * The function \a leak_finder should returns leaks found from HprofHeap which are wanted to be tracked, The
         * function will returns the same amount of LeakChain instances as leaks, each of which corresponds to a leak,
         * or amount of LeakChain instances is less than leaks count when there is a "leak" did not being referred by
         * GC roots.
         */
        std::vector<LeakChain> Analyze(const std::function<std::vector<object_id_t>(const HprofHeap &)> &leak_finder);

    private:
        friend_test(main_analyzer, construct);

        friend_test(main_analyzer, construct_error_handle);

        friend_test(main_analyzer, exclude_matchers);

        friend_test(main_analyzer, analyze);

        std::unique_ptr<HprofAnalyzerImpl> impl_;
    };
}

#endif