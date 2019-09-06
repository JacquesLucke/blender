#pragma once

#include "FN_core.hpp"
#include "FN_cpp.hpp"

namespace FN {
namespace Types {

using StringW = UniquePointerWrapper<std::string>;

void INIT_string(Vector<Type *> &types_to_free);

extern Type *TYPE_string;
extern Type *TYPE_string_list;

}  // namespace Types
}  // namespace FN
