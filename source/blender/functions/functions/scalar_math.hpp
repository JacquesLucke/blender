#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

SharedFunction &GET_FN_add_floats();
SharedFunction &GET_FN_multiply_floats();
SharedFunction &GET_FN_min_floats();
SharedFunction &GET_FN_max_floats();
SharedFunction &GET_FN_map_range();
SharedFunction &GET_FN_sin_float();

SharedFunction &GET_FN_output_int32_0();
SharedFunction &GET_FN_output_int32_1();
SharedFunction &GET_FN_output_float_0();
SharedFunction &GET_FN_output_float_1();
SharedFunction &GET_FN_output_false();
SharedFunction &GET_FN_output_true();

}  // namespace Functions
}  // namespace FN
