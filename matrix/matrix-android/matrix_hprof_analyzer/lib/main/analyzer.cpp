#include "internal/main_analyzer.h"
#include "internal/main_chain.h"
#include "internal/main_heap.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <utility>
#include <cerrno>

#include "analyzer.h"

namespace matrix::hprof {

    // public interface

    HprofAnalyzer::HprofAnalyzer(int hprof_fd) : impl_(new HprofAnalyzerImpl(hprof_fd)) {}

    HprofAnalyzer::~HprofAnalyzer() = default;

    void HprofAnalyzer::ExcludeInstanceFieldReference(const std::string &class_name,
                                                      const std::string &field_name) {
        impl_->ExcludeInstanceFieldReference(class_name, field_name);
    }

    void HprofAnalyzer::ExcludeStaticFieldReference(const std::string &class_name,
                                                    const std::string &field_name) {
        impl_->ExcludeStaticFieldReference(class_name, field_name);
    }

    void HprofAnalyzer::ExcludeThreadReference(const std::string &thread_name) {
        impl_->ExcludeThreadReference(thread_name);
    }

    void HprofAnalyzer::ExcludeNativeGlobalReference(const std::string &class_name) {
        impl_->ExcludeNativeGlobalReference(class_name);
    }

    std::vector<LeakChain>
    HprofAnalyzer::Analyze(const std::function<std::vector<object_id_t>(const HprofHeap &)> &leak_finder) {
        return impl_->Analyze(leak_finder);
    }


    // implementation

    HprofAnalyzerImpl::HprofAnalyzerImpl(int hprof_fd) : parser_(new internal::parser::HeapParser()) {
        struct stat file_stat{};
        if (fstat(hprof_fd, &file_stat)) {
            std::string error_msg =
                    "Failed to check state of file descriptor. Error code: " + std::to_string(errno) + ".";
            throw std::runtime_error(error_msg);
        }
        if (!S_ISREG(file_stat.st_mode))
            throw std::runtime_error("File descriptor is a regular file.");
        data_size_ = file_stat.st_size;
        data_ = mmap(nullptr, data_size_, PROT_READ, MAP_PRIVATE, hprof_fd, 0);
        if (data_ == nullptr) throw std::runtime_error("Failed to mmap file.");
    }

    HprofAnalyzerImpl::~HprofAnalyzerImpl() {
        munmap(data_, data_size_);
    }

    void HprofAnalyzerImpl::ExcludeInstanceFieldReference(const std::string &class_name,
                                                          const std::string &field_name) {
        exclude_matcher_group_.instance_fields_.emplace_back(
                internal::parser::FieldExcludeMatcher(class_name, field_name));
    }

    void HprofAnalyzerImpl::ExcludeStaticFieldReference(const std::string &class_name,
                                                        const std::string &field_name) {
        exclude_matcher_group_.static_fields_.emplace_back(
                internal::parser::FieldExcludeMatcher(class_name, field_name));
    }

    void HprofAnalyzerImpl::ExcludeThreadReference(const std::string &thread_name) {
        exclude_matcher_group_.threads_.emplace_back(
                internal::parser::ThreadExcludeMatcher(thread_name));
    }

    void HprofAnalyzerImpl::ExcludeNativeGlobalReference(const std::string &class_name) {
        exclude_matcher_group_.native_globals_.emplace_back(
                internal::parser::NativeGlobalExcludeMatcher(class_name));
    }

    std::vector<LeakChain>
    HprofAnalyzerImpl::Analyze(const std::function<std::vector<object_id_t>(const HprofHeap &)> &leak_finder) {
        internal::heap::Heap heap;
        internal::reader::Reader reader(reinterpret_cast<const uint8_t *>(data_), data_size_);
        parser_->Parse(reader, heap, exclude_matcher_group_);
        return Analyze(heap, leak_finder(HprofHeap(new HprofHeapImpl(heap))));
    }

    std::vector<LeakChain> HprofAnalyzerImpl::Analyze(const internal::heap::Heap &heap,
                                                      const std::vector<object_id_t> &leaks) {
        const auto chains = ({
            const HprofHeap hprof_heap(new HprofHeapImpl(heap));
            internal::analyzer::find_leak_chains(heap, leaks);
        });
        std::vector<LeakChain> result;
        for (const auto&[_, chain]: chains) {
            const std::optional<LeakChain> leak_chain = BuildLeakChain(heap, chain);
            if (leak_chain.has_value()) result.emplace_back(leak_chain.value());
        }
        return std::move(result);
    }

    std::optional<LeakChain> HprofAnalyzerImpl::BuildLeakChain(const internal::heap::Heap &heap,
                                                               const std::vector<std::pair<internal::heap::object_id_t, std::optional<internal::heap::reference_t>>> &chain) {
        if (chain.empty()) return std::nullopt;
        std::optional<LeakChain::GcRoot> gc_root;
        std::vector<LeakChain::Node> nodes;
        internal::heap::reference_t next_reference{};
        for (size_t i = 0; i < chain.size(); ++i) {
            const auto &chain_node = chain[i];
            const std::string referent = ({
                const object_id_t class_id = (chain_node.second.has_value() &&
                                              chain_node.second.value().type == internal::heap::kStaticField)
                                             ? chain_node.first
                                             : unwrap(heap.GetClass(chain_node.first), return std::nullopt);
                unwrap(heap.GetClassName(class_id), return std::nullopt);
            });
            if (i == 0) {
                const LeakChain::GcRoot::Type gc_root_type =
                        convert_gc_root_type(heap.GetGcRootType(chain_node.first));
                gc_root = create_leak_chain_gc_root(referent, gc_root_type);
            } else {
                const std::string reference = next_reference.type == internal::heap::kArrayElement
                                              ? std::to_string(next_reference.index)
                                              : heap.GetString(next_reference.field_name_id);
                const LeakChain::Node::ReferenceType reference_type =
                        convert_reference_type(next_reference.type);
                const LeakChain::Node::ObjectType referent_type =
                        convert_object_type(heap.GetInstanceType(chain_node.first));
                nodes.emplace_back(create_leak_chain_node(reference, reference_type, referent, referent_type));
            }
            next_reference = unwrap(chain_node.second, break);
        }
        return create_leak_chain(gc_root.value(), nodes);
    }
}