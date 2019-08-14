#pragma once

#include "../FN_core.hpp"
#include "BLI_math.hpp"

namespace FN {
namespace Types {

using BLI::float3;
using BLI::rgba_f;

Type *&GET_TYPE_float();
Type *&GET_TYPE_int32();
Type *&GET_TYPE_float3();
Type *&GET_TYPE_rgba_f();

}  // namespace Types
}  // namespace FN
