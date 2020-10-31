/* Apache License, Version 2.0 */

#include "FN_lang_multi_function.hh"
#include "FN_multi_function_eval_utils.hh"

#include "testing/testing.h"

namespace blender::fn::lang::tests {

static std::unique_ptr<MFSymbolTable> create_symbol_table()
{
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
      "5", MFDataType::ForSingle<int>(), {}, resources, symbols);

  const int result = mf_eval_1_SO<int>(fn);
  EXPECT_EQ(result, 5);
}

TEST(fn_lang_expression, AddConstants)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "3+6+10", MFDataType::ForSingle<int>(), {}, resources, symbols);
  const int result = mf_eval_1_SO<int>(fn);
  EXPECT_EQ(result, 19);
}

TEST(fn_lang_expression, RepeatString)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function(
      "\"hello\" * (2 + 3)", MFDataType::ForSingle<std::string>(), {}, resources, symbols);
  const std::string result = mf_eval_1_SO<std::string>(fn);
  EXPECT_EQ(result, "hellohellohellohellohello");
}

TEST(fn_lang_expression, AddToVariable)
{
  MFSymbolTable &symbols = get_symbol_table();
  ResourceCollector resources;
  const MultiFunction &fn = expression_to_multi_function("var + 4",
                                                         MFDataType::ForSingle<int>(),
                                                         {{MFDataType::ForSingle<int>(), "var"}},
                                                         resources,
                                                         symbols);
  const int result = mf_eval_1_SI_SO<int, int>(fn, 10);
  EXPECT_EQ(result, 14);
}

}  // namespace blender::fn::lang::tests
