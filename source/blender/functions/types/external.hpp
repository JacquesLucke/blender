#pragma once

#include "FN_core.hpp"
#include "FN_cpp.hpp"

struct Object;

namespace FN {
namespace Types {

using ObjectW = ReferencedPointerWrapper<Object>;

void INIT_external(Vector<Type *> &types_to_free);

extern Type *TYPE_object;
extern Type *TYPE_object_list;

}  // namespace Types
}  // namespace FN
