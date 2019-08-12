#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

SharedFunction &GET_FN_add_floats();
SharedFunction &GET_FN_sub_floats();
SharedFunction &GET_FN_multiply_floats();
SharedFunction &GET_FN_divide_floats();

SharedFunction &GET_FN_power_floats();
SharedFunction &GET_FN_log_floats();
SharedFunction &GET_FN_sqrt_float();

SharedFunction &GET_FN_abs_float();
SharedFunction &GET_FN_min_floats();
SharedFunction &GET_FN_max_floats();
SharedFunction &GET_FN_mod_floats();

SharedFunction &GET_FN_sin_float();
SharedFunction &GET_FN_cos_float();
SharedFunction &GET_FN_tan_float();
SharedFunction &GET_FN_asin_float();
SharedFunction &GET_FN_acos_float();
SharedFunction &GET_FN_atan_float();
SharedFunction &GET_FN_atan2_floats();

SharedFunction &GET_FN_fract_float();
SharedFunction &GET_FN_ceil_float();
SharedFunction &GET_FN_floor_float();
SharedFunction &GET_FN_round_float();
SharedFunction &GET_FN_snap_floats();

SharedFunction &GET_FN_map_range();

}  // namespace Functions
}  // namespace FN
