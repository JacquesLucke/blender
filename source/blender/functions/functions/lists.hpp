#pragma once

#include "FN_core.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Functions {

Type *get_list_type(Type *base_type);

Function &GET_FN_empty_list(Type *base_type);
Function &GET_FN_list_from_element(Type *base_type);
Function &GET_FN_append_to_list(Type *base_type);
Function &GET_FN_get_list_element(Type *base_type);
Function &GET_FN_combine_lists(Type *base_type);
Function &GET_FN_list_length(Type *base_type);

}  // namespace Functions
}  // namespace FN
