#include "global_functions.h"
#include "FN_multi_functions.h"

namespace FN {

const MultiFunction *MF_GLOBAL_add_floats_2 = nullptr;
const MultiFunction *MF_GLOBAL_multiply_floats_2 = nullptr;
const MultiFunction *MF_GLOBAL_subtract_floats = nullptr;
const MultiFunction *MF_GLOBAL_safe_division_floats = nullptr;

void init_global_functions()
{
  MF_GLOBAL_add_floats_2 = new MF_Custom_In2_Out1<float, float, float>(
      "add 2 floats", [](float a, float b) { return a + b; }, BLI_RAND_PER_LINE_UINT32);
  MF_GLOBAL_multiply_floats_2 = new MF_Custom_In2_Out1<float, float, float>(
      "multiply 2 floats", [](float a, float b) { return a * b; }, BLI_RAND_PER_LINE_UINT32);
  MF_GLOBAL_subtract_floats = new MF_Custom_In2_Out1<float, float, float>(
      "subtract 2 floats", [](float a, float b) { return a - b; }, BLI_RAND_PER_LINE_UINT32);
  MF_GLOBAL_safe_division_floats = new MF_Custom_In2_Out1<float, float, float>(
      "safe divide 2 floats",
      [](float a, float b) { return (b != 0.0f) ? a / b : 0.0f; },
      BLI_RAND_PER_LINE_UINT32);
}

void free_global_functions()
{
  delete MF_GLOBAL_add_floats_2;
  delete MF_GLOBAL_multiply_floats_2;
  delete MF_GLOBAL_subtract_floats;
  delete MF_GLOBAL_safe_division_floats;
}

}  // namespace FN
