#pragma once

#include "FN_core.hpp"

namespace FN {
namespace Functions {

Function &to_vectorized_function__with_cache(Function &fn,
                                             ArrayRef<bool> vectorized_inputs_mask,
                                             ArrayRef<Function *> empty_list_value_builders);

std::unique_ptr<Function> to_vectorized_function__without_cache(
    Function &fn,
    ArrayRef<bool> vectorized_inputs_mask,
    ArrayRef<Function *> empty_list_value_builders);

}  // namespace Functions
}  // namespace FN
