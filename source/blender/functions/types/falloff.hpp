#pragma once

#include "FN_core.hpp"
#include "FN_cpp.hpp"

namespace BKE {
class Falloff;
};

namespace FN {
namespace Types {

using FalloffW = OwningPointerWrapper<BKE::Falloff>;

void INIT_falloff(Vector<Type *> &types_to_free);

extern Type *TYPE_falloff;
extern Type *TYPE_falloff_list;

}  // namespace Types
}  // namespace FN
