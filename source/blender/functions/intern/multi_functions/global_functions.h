#pragma once

#include "FN_multi_function.h"

namespace FN {

void init_global_functions();
void free_global_functions();

extern const MultiFunction *MF_GLOBAL_add_floats_2;
extern const MultiFunction *MF_GLOBAL_multiply_floats_2;
extern const MultiFunction *MF_GLOBAL_subtract_floats;
extern const MultiFunction *MF_GLOBAL_safe_division_floats;

}  // namespace FN
