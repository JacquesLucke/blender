#include "global_functions.h"
#include "FN_multi_functions.h"

namespace FN {

const MultiFunction *MF_GLOBAL_add_floats_2 = nullptr;
const MultiFunction *MF_GLOBAL_add_int32s_2 = nullptr;
const MultiFunction *MF_GLOBAL_multiply_floats_2 = nullptr;
const MultiFunction *MF_GLOBAL_subtract_floats = nullptr;
const MultiFunction *MF_GLOBAL_safe_division_floats = nullptr;
const MultiFunction *MF_GLOBAL_sin_float = nullptr;
const MultiFunction *MF_GLOBAL_cos_float = nullptr;

void init_global_functions()
{
  MF_GLOBAL_add_floats_2 = new MF_Custom_In2_Out1<float, float, float>(
      "add 2 floats", [](float a, float b) { return a + b; });
  MF_GLOBAL_add_int32s_2 = new MF_Custom_In2_Out1<int32_t, int32_t, int32_t>(
      "add 2 int32s", [](int32_t a, int32_t b) { return a + b; });
  MF_GLOBAL_multiply_floats_2 = new MF_Custom_In2_Out1<float, float, float>(
      "multiply 2 floats", [](float a, float b) { return a * b; });
  MF_GLOBAL_subtract_floats = new MF_Custom_In2_Out1<float, float, float>(
      "subtract 2 floats", [](float a, float b) { return a - b; });
  MF_GLOBAL_safe_division_floats = new MF_Custom_In2_Out1<float, float, float>(
      "safe divide 2 floats", [](float a, float b) { return (b != 0.0f) ? a / b : 0.0f; });

  MF_GLOBAL_sin_float = new MF_Custom_In1_Out1<float, float>("sin float",
                                                             [](float a) { return std::sin(a); });
  MF_GLOBAL_cos_float = new MF_Custom_In1_Out1<float, float>("cos float",
                                                             [](float a) { return std::cos(a); });
}

void free_global_functions()
{
  delete MF_GLOBAL_add_floats_2;
  delete MF_GLOBAL_add_int32s_2;
  delete MF_GLOBAL_multiply_floats_2;
  delete MF_GLOBAL_subtract_floats;
  delete MF_GLOBAL_safe_division_floats;
  delete MF_GLOBAL_sin_float;
  delete MF_GLOBAL_cos_float;
}

}  // namespace FN
