#include "include/analyzer.h"

#include <queue>

namespace matrix::hprof::internal::analyzer {

    struct ref_super_t {
        heap::object_id_t referrer_id;
        heap::reference_t reference;
    };

    struct ref_node_t {
        heap::object_id_t referent_id;
        std::optional<ref_super_t> super;
        size_t depth;
    };

    std::map<heap::object_id_t, std::vector<std::pair<heap::object_id_t, std::optional<heap::reference_t>>>>
    find_leak_chains(const heap::Heap &heap, const std::vector<heap::object_id_t> &tracked) {

        /* Use Breadth-First Search algorithm to find all the references to tracked objects. */

        std::map<heap::object_id_t, std::vector<std::pair<heap::object_id_t, std::optional<heap::reference_t>>>> ret;

        for (const auto &leak: tracked) {
            std::map<heap::object_id_t, ref_node_t> traversed;
            std::deque<ref_node_t> waiting;

            for (const heap::object_id_t gc_root: heap.GetGcRoots()) {
                ref_node_t node = {
                        .referent_id = gc_root,
                        .super = std::nullopt,
                        .depth = 0
                };
                traversed[gc_root] = node;
                waiting.push_back(node);
            }

            bool found = false;
            while (!waiting.empty()) {
                const ref_node_t node = waiting.front();
                waiting.pop_front();
                const heap::object_id_t referrer_id = node.referent_id;
                if (heap.GetLeakReferenceGraph().count(referrer_id) == 0) continue;
                for (const auto &[referent, reference]: heap.GetLeakReferenceGraph().at(referrer_id)) {
                    try {
                        if (traversed.at(referent).depth <= node.depth + 1) continue;
                    } catch (const std::out_of_range &) {}
                    ref_node_t next_node = {
                            .referent_id = referent,
                            .super = ref_super_t{
                                    .referrer_id = referrer_id,
                                    .reference = reference
                            },
                            .depth = node.depth + 1
                    };
                    traversed[referent] = next_node;
                    if (leak == referent) {
                        found = true;
                        goto traverse_complete;
                    } else {
                        waiting.push_back(next_node);
                    }
                }
            }
            traverse_complete:
            if (found) {
                ret[leak] = std::vector<std::pair<heap::object_id_t, std::optional<heap::reference_t>>>();
                std::optional<heap::object_id_t> current = leak;
                std::optional<heap::reference_t> current_reference = std::nullopt;
                while (current != std::nullopt) {
                    ret[leak].push_back(std::make_pair(current.value(), current_reference));
                    const auto &super = traversed.at(current.value()).super;
                    if (super.has_value()) {
                        current = super.value().referrer_id;
                        current_reference = super.value().reference;
                    } else {
                        current = std::nullopt;
                    }
                }
                std::reverse(ret[leak].begin(), ret[leak].end());
            }
        }

        return std::move(ret);
    }
}

