#pragma once

#include "FN_cpp.hpp"

namespace FN {
namespace Types {

SharedType new_list_type(SharedType &base_type);

SharedType &GET_TYPE_float_list();
SharedType &GET_TYPE_float3_list();
SharedType &GET_TYPE_int32_list();
SharedType &GET_TYPE_bool_list();
SharedType &GET_TYPE_object_list();
SharedType &GET_TYPE_rgba_f_list();

}  // namespace Types
}  // namespace FN
