#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

SharedFunction to_vectorized_function(SharedFunction &fn,
                                      ArrayRef<bool> vectorized_inputs_mask,
                                      ArrayRef<SharedFunction> empty_list_value_builders);

SharedFunction to_vectorized_function__with_cache(
    SharedFunction &fn,
    ArrayRef<bool> vectorized_inputs_mask,
    ArrayRef<SharedFunction> empty_list_value_builders);

}  // namespace Functions
}  // namespace FN
