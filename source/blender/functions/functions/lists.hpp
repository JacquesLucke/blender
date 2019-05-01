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

typedef uint (*GetListLength)(void *list);
typedef void *(*GetListDataPtr)(void *list);
typedef void *(*NewListWithAllocatedBuffer)(uint length);

GetListLength GET_C_FN_list_length(SharedType &base_type);
GetListDataPtr GET_C_FN_list_data_ptr(SharedType &base_type);
NewListWithAllocatedBuffer GET_C_FN_new_list_with_allocated_buffer(SharedType &base_type);

}  // namespace Functions
}  // namespace FN
