#pragma once

#include "../FN_core.hpp"
#include "BLI_math.hpp"

namespace FN {
namespace Types {

using BLI::float3;
using BLI::rgba_f;

void INIT_numeric();

extern Type *TYPE_float;
extern Type *TYPE_int32;
extern Type *TYPE_float3;
extern Type *TYPE_rgba_f;

extern Type *TYPE_float_list;
extern Type *TYPE_int32_list;
extern Type *TYPE_float3_list;
extern Type *TYPE_rgba_f_list;

}  // namespace Types
}  // namespace FN
