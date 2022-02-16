#ifndef __matrix_hprof_analyzer_internal_chain_h__
#define __matrix_hprof_analyzer_internal_chain_h__

#include "../include/matrix_hprof_analyzer.h"

#include "heap.h"

namespace matrix::hprof {

    LeakChain::GcRoot::Type convert_gc_root_type(internal::heap::gc_root_type_t type);

    LeakChain::Node::ReferenceType convert_reference_type(internal::heap::reference_type_t type);

    LeakChain::Node::ObjectType convert_object_type(internal::heap::object_type_t type);
}

#endif