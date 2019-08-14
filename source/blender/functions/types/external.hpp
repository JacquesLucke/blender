#pragma once

#include "../FN_core.hpp"

namespace FN {
namespace Types {

void INIT_external(Vector<Type *> &types_to_free);

extern Type *TYPE_object;
extern Type *TYPE_object_list;

}  // namespace Types
}  // namespace FN
