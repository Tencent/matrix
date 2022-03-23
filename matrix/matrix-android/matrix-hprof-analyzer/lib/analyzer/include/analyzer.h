#ifndef __matrix_hprof_analyzer_analyzer_h__
#define __matrix_hprof_analyzer_analyzer_h__

#include "heap.h"

#include <map>
#include <vector>
#include <optional>

namespace matrix::hprof::internal::analyzer {

    /**
     * The key of returned map is the leak identifier in \a tracked, or the identifier is not in map keys if the object
     * is not a leak.
     * <p>
     * The value of returned map is a list of referrer-reference pair. The reference of last list element must be
     * std::nullopt and the referrer is the leak.
     */
    std::map<heap::object_id_t, std::vector<std::pair<heap::object_id_t, std::optional<heap::reference_t>>>>
    find_leak_chains(const heap::Heap &heap, const std::vector<heap::object_id_t>& tracked);
}

#endif