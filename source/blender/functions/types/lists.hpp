#pragma once

#include "FN_cpp.hpp"

namespace FN {
namespace Types {

Type *new_list_type(Type *base_type);

Type *&GET_TYPE_float_list();
Type *&GET_TYPE_float3_list();
Type *&GET_TYPE_int32_list();
Type *&GET_TYPE_bool_list();
Type *&GET_TYPE_object_list();
Type *&GET_TYPE_rgba_f_list();

}  // namespace Types
}  // namespace FN
