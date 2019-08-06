#pragma once

#include "FN_core.hpp"
#include "FN_types.hpp"

namespace FN {
namespace Functions {

SharedType &get_list_type(SharedType &base_type);

SharedFunction &GET_FN_empty_list(SharedType &base_type);
SharedFunction &GET_FN_list_from_element(SharedType &base_type);
SharedFunction &GET_FN_append_to_list(SharedType &base_type);
SharedFunction &GET_FN_get_list_element(SharedType &base_type);
SharedFunction &GET_FN_combine_lists(SharedType &base_type);
SharedFunction &GET_FN_list_length(SharedType &base_type);

}  // namespace Functions
}  // namespace FN
