#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

Function &GET_FN_add_floats();
Function &GET_FN_sub_floats();
Function &GET_FN_multiply_floats();
Function &GET_FN_divide_floats();

Function &GET_FN_power_floats();
Function &GET_FN_log_floats();
Function &GET_FN_sqrt_float();

Function &GET_FN_abs_float();
Function &GET_FN_min_floats();
Function &GET_FN_max_floats();
Function &GET_FN_mod_floats();

Function &GET_FN_sin_float();
Function &GET_FN_cos_float();
Function &GET_FN_tan_float();
Function &GET_FN_asin_float();
Function &GET_FN_acos_float();
Function &GET_FN_atan_float();
Function &GET_FN_atan2_floats();

Function &GET_FN_fract_float();
Function &GET_FN_ceil_float();
Function &GET_FN_floor_float();
Function &GET_FN_round_float();
Function &GET_FN_snap_floats();

Function &GET_FN_map_range();

}  // namespace Functions
}  // namespace FN
