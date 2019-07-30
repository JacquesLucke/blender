#pragma once

#include "../FN_core.hpp"
#include "BLI_math.hpp"

namespace FN {
namespace Types {

using BLI::float3;
using BLI::rgba_f;

SharedType &GET_TYPE_float();
SharedType &GET_TYPE_int32();
SharedType &GET_TYPE_float3();
SharedType &GET_TYPE_rgba_f();

}  // namespace Types
}  // namespace FN
