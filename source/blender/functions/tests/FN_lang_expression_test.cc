/* Apache License, Version 2.0 */

#include "FN_lang_multi_function.hh"
#include "FN_multi_function_eval_utils.hh"

#include "BLI_float3.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

static std::unique_ptr<MFSymbolTable> create_symbol_table()
{
  static ResourceCollector resources;
  std::unique_ptr<MFSymbolTable> symbols = std::make_unique<MFSymbolTable>();

  static CustomMF_SI_SI_SO<int, int, int> add_ints_fn{"Add Ints",
                                                      [](int a, int b) { return a + b; }};
  symbols->add_function("a+b", add_ints_fn);

  static CustomMF_SI_SI_SO<std::string, int, std::string> repeat_string_fn{
      "Repeat String", [](const std::string &str, int times) {
        std::string new_string;
        for (int i = 0; i < times; i++) {
          new_string += str;
        }
        return new_string;
      }};
  symbols->add_function("a*b", repeat_string_fn);

  static CustomMF_SI_SO<float, float3> float_to_float3_fn{"float to float3",
                                                          [](float a) { return float3(a, a, a); }};
  symbols->add_function("float3", float_to_float3_fn);

  static CustomMF_SI_SI_SI_SO<float, float, float, float3> make_float3_fn{
      "make float3", [](float a, float b, float c) { return float3(a, b, c); }};
  symbols->add_function("float3", make_float3_fn);

  static CustomMF_SI_SI_SO<float3, float3, float3> add_float3_fn{
      "add float3", [](float3 a, float3 b) { return a + b; }};
  symbols->add_function("a+b", add_float3_fn);

  static CustomMF_SI_SI_SO<float3, float, float3> scale_float3_fn{
      "scale float3", [](float3 vec, float fac) { return vec * fac; }};
  symbols->add_function("a*b", scale_float3_fn);

  symbols->add_conversion<int, float>(resources);
  symbols->add_conversion<float, int>(resources);

  return symbols;
}

static MFSymbolTable &get_symbol_table()
{
  static std::unique_ptr<MFSymbolTable> symbols = create_symbol_table();
  return *symbols;
}

TEST(fn_lang_expression, SingleConstant)
{
  MFSymbolTable symbols;
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "5", symbols, resources, MFDataType::ForSingle<int>());

  const int result = mf_eval_1_SO<int>(fn);
  EXPECT_EQ(result, 5);
}

TEST(fn_lang_expression, AddConstants)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "3+6+10", symbols, resources, MFDataType::ForSingle<int>());
  const int result = mf_eval_1_SO<int>(fn);
  EXPECT_EQ(result, 19);
}

TEST(fn_lang_expression, RepeatString)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "\"hello\" * (2 + 3)", symbols, resources, MFDataType::ForSingle<std::string>());
  const std::string result = mf_eval_1_SO<std::string>(fn);
  EXPECT_EQ(result, "hellohellohellohellohello");
}

TEST(fn_lang_expression, AddToVariable)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function("var + 4",
                                                         symbols,
                                                         resources,
                                                         MFDataType::ForSingle<int>(),
                                                         {{MFDataType::ForSingle<int>(), "var"}});
  const int result = mf_eval_1_SI_SO<int, int>(fn, 10);
  EXPECT_EQ(result, 14);
}

TEST(fn_lang_expression, UseUndefinedVariable)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  EXPECT_ANY_THROW(
      expression_to_multi_function("var + 4", symbols, resources, MFDataType::ForSingle<int>()));
}

TEST(fn_lang_expression, SimpleVectorMath)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function("(float3(a, 2, 3) + float3(a)) * 10",
                                                         symbols,
                                                         resources,
                                                         MFDataType::ForSingle<float3>(),
                                                         {{MFDataType::ForSingle<float>(), "a"}});
  const float3 result1 = mf_eval_1_SI_SO<float, float3>(fn, 3);
  EXPECT_EQ(result1, float3(60, 50, 60));
  const float3 result2 = mf_eval_1_SI_SO<float, float3>(fn, 0);
  EXPECT_EQ(result2, float3(0, 20, 30));
}

}  // namespace blender::fn::lang::tests
