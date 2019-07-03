#pragma once

#include "../FN_core.hpp"
#include "BLI_math.hpp"

namespace FN {
namespace Types {

using BLI::float3;

SharedType &GET_TYPE_float();
SharedType &GET_TYPE_int32();
SharedType &GET_TYPE_float3();

}  // namespace Types
}  // namespace FN
