#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

Function &GET_FN_bool_to_int32();
Function &GET_FN_bool_to_float();
Function &GET_FN_int32_to_float();
Function &GET_FN_int32_to_bool();
Function &GET_FN_float_to_int32();
Function &GET_FN_float_to_bool();

Function &GET_FN_bool_list_to_int32_list();
Function &GET_FN_bool_list_to_float_list();
Function &GET_FN_int32_list_to_float_list();
Function &GET_FN_int32_list_to_bool_list();
Function &GET_FN_float_list_to_int32_list();
Function &GET_FN_float_list_to_bool_list();

}  // namespace Functions
}  // namespace FN
