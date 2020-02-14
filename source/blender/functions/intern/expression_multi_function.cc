#include "FN_expression_multi_function.h"
#include "FN_expression_parser.h"
#include "FN_multi_function_network.h"
#include "FN_multi_functions.h"

#include "BLI_string_map.h"

namespace FN {
namespace Expr {

using BLI::StringMap;

class AstToNetworkBuilder {
 private:
  MFNetworkBuilder &m_network_builder;
  ResourceCollector &m_resources;
  const StringMap<MFBuilderOutputSocket *> &m_expression_inputs;
  const ConstantsTable &m_constants_table;
  const FunctionTable &m_function_table;
  const ConversionTable &m_conversion_table;

 public:
  AstToNetworkBuilder(MFNetworkBuilder &network_builder,
                      ResourceCollector &resources,
                      const StringMap<MFBuilderOutputSocket *> &expression_inputs,
                      const ConstantsTable &constants_table,
                      const FunctionTable &function_table,
                      const ConversionTable &conversion_table)
      : m_network_builder(network_builder),
        m_resources(resources),
        m_expression_inputs(expression_inputs),
        m_constants_table(constants_table),
        m_function_table(function_table),
        m_conversion_table(conversion_table)
  {
  }

  MFBuilderOutputSocket &build(AstNode &ast_node)
  {
    switch (ast_node.type) {
      case AstNodeType::Less:
        return this->insert_binary_function("a<b", ast_node);
      case AstNodeType::Greater:
        return this->insert_binary_function("a>b", ast_node);
      case AstNodeType::Equal:
        return this->insert_binary_function("a==b", ast_node);
      case AstNodeType::LessOrEqual:
        return this->insert_binary_function("a<=b", ast_node);
      case AstNodeType::GreaterOrEqual:
        return this->insert_binary_function("a>=b", ast_node);
      case AstNodeType::Plus:
        return this->insert_binary_function("a+b", ast_node);
      case AstNodeType::Minus:
        return this->insert_binary_function("a-b", ast_node);
      case AstNodeType::Multiply:
        return this->insert_binary_function("a*b", ast_node);
      case AstNodeType::Divide:
        return this->insert_binary_function("a/b", ast_node);
      case AstNodeType::Identifier: {
        IdentifierNode &identifier_node = (IdentifierNode &)ast_node;
        StringRef identifier = identifier_node.value;
        MFBuilderOutputSocket *expression_input_socket = m_expression_inputs.lookup_default(
            identifier, nullptr);
        if (expression_input_socket != nullptr) {
          return *expression_input_socket;
        }
        Optional<SingleConstant> constant = m_constants_table.try_lookup(identifier);
        BLI_assert(constant.has_value());
        return m_network_builder
            .add_function<MF_GenericConstantValue>(m_resources, *constant->type, constant->buffer)
            .output(0);
      }
      case AstNodeType::ConstantInt: {
        ConstantIntNode &int_node = (ConstantIntNode &)ast_node;
        MFBuilderFunctionNode &node = m_network_builder.add_function<MF_ConstantValue<int>>(
            m_resources, int_node.value);
        return node.output(0);
      }
      case AstNodeType::ConstantFloat: {
        ConstantFloatNode &float_node = (ConstantFloatNode &)ast_node;
        MFBuilderFunctionNode &node = m_network_builder.add_function<MF_ConstantValue<float>>(
            m_resources, float_node.value);
        return node.output(0);
      }
      case AstNodeType::ConstantString: {
        ConstantStringNode &string_node = (ConstantStringNode &)ast_node;
        MFBuilderFunctionNode &node =
            m_network_builder.add_function<MF_ConstantValue<std::string>>(m_resources,
                                                                          string_node.value);
        return node.output(0);
      }
      case AstNodeType::Negate: {
        MFBuilderOutputSocket &sub = this->build(*ast_node.children[0]);
        return this->insert_function("-a", {&sub});
      }
      case AstNodeType::Power: {
        return this->insert_binary_function("a**b", ast_node);
      }
      case AstNodeType::Call: {
        CallNode &call_node = (CallNode &)ast_node;
        Vector<MFBuilderOutputSocket *> arg_sockets;
        for (AstNode *child : ast_node.children) {
          arg_sockets.append(&this->build(*child));
        }
        return this->insert_function(call_node.name, arg_sockets);
      }
    }
    BLI_assert(false);
    return this->build(ast_node);
  }

  MFBuilderOutputSocket &insert_binary_function(StringRef name, AstNode &ast_node)
  {
    MFBuilderOutputSocket *sub1 = &this->build(*ast_node.children[0]);
    MFBuilderOutputSocket *sub2 = &this->build(*ast_node.children[1]);
    return this->insert_function(name, {sub1, sub2});
  }

  MFBuilderOutputSocket &insert_function(StringRef name,
                                         ArrayRef<MFBuilderOutputSocket *> arg_sockets)
  {
    Vector<MFDataType> arg_types;
    for (MFBuilderOutputSocket *socket : arg_sockets) {
      arg_types.append(socket->data_type());
    }
    const MultiFunction &fn = this->lookup_function(name, arg_types);
    MFBuilderNode &node = m_network_builder.add_function(fn);
    BLI_assert(node.inputs().size() == arg_sockets.size());
    for (uint i : arg_sockets.index_range()) {
      MFDataType actual_type = arg_types[i];
      MFDataType expected_type = node.input(i).data_type();
      if (actual_type == expected_type) {
        m_network_builder.add_link(*arg_sockets[i], node.input(i));
      }
      else {
        const MultiFunction &conversion_fn = *m_conversion_table.try_lookup(actual_type,
                                                                            expected_type);
        MFBuilderNode &conversion_node = m_network_builder.add_function(conversion_fn);
        m_network_builder.add_link(*arg_sockets[i], conversion_node.input(0));
        m_network_builder.add_link(conversion_node.output(0), node.input(i));
      }
    }
    return node.output(0);
  }

  const MultiFunction &lookup_function(StringRef name, ArrayRef<MFDataType> arg_types)
  {
    ArrayRef<const MultiFunction *> candidates = m_function_table.lookup(name);
    const MultiFunction *best_fit_yet = nullptr;
    int best_suitability_yet = INT32_MAX;
    for (const MultiFunction *candidate : candidates) {
      int suitability = this->get_function_suitability(*candidate, arg_types);
      if (suitability >= 0) {
        if (suitability < best_suitability_yet) {
          best_fit_yet = candidate;
          best_suitability_yet = suitability;
        }
      }
    }
    BLI_assert(best_fit_yet != nullptr);
    return *best_fit_yet;
  }

  /* Return -1, when the function cannot be used. Otherwise, lower return values mean a better fit.
   */
  int get_function_suitability(const MultiFunction &fn, ArrayRef<MFDataType> arg_types)
  {
    uint input_index = 0;
    int conversion_count = 0;
    for (uint param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      if (param_type.is_input_or_mutable()) {
        if (input_index >= arg_types.size()) {
          /* Number of arguments does not match. */
          return -1;
        }

        MFDataType actual_type = arg_types[input_index];
        MFDataType expected_type = param_type.data_type();
        if (actual_type != expected_type) {
          if (m_conversion_table.can_convert(actual_type, expected_type)) {
            conversion_count++;
          }
          else {
            return -1;
          }
        }
        input_index++;
      }
    }
    if (input_index != arg_types.size()) {
      /* Number of arguments does not match. */
      return -1;
    }
    return conversion_count;
  }
};

const MultiFunction &expression_to_multi_function(StringRef str,
                                                  ResourceCollector &resources,
                                                  ArrayRef<StringRef> variable_names,
                                                  ArrayRef<MFDataType> variable_types,
                                                  const ConstantsTable &constants_table,
                                                  const FunctionTable &function_table,
                                                  const ConversionTable &conversion_table)
{
  BLI::assert_same_size(variable_names, variable_types);
  AstNode &ast_node = parse_expression(str, resources.allocator());

  MFNetworkBuilder network_builder;
  StringMap<MFBuilderOutputSocket *> builder_dummy_inputs;
  for (uint i : variable_names.index_range()) {
    StringRef identifier = variable_names[i];
    MFBuilderDummyNode &node = network_builder.add_dummy(
        identifier, {}, {variable_types[i]}, {}, {"Value"});
    builder_dummy_inputs.add_new(identifier, &node.output(0));
  }

  AstToNetworkBuilder builder{network_builder,
                              resources,
                              builder_dummy_inputs,
                              constants_table,
                              function_table,
                              conversion_table};
  MFBuilderOutputSocket &builder_output_socket = builder.build(ast_node);

  MFBuilderDummyNode &builder_output = network_builder.add_output_dummy("Result",
                                                                        builder_output_socket);

  MFNetwork &network = resources.construct<MFNetwork>("expression network", network_builder);
  const MFInputSocket &output_socket = network.find_dummy_socket(builder_output.input(0));

  Vector<const MFOutputSocket *> inputs;
  builder_dummy_inputs.foreach_value([&](MFBuilderOutputSocket *builder_input) {
    inputs.append(&network.find_dummy_socket(*builder_input));
  });
  Vector<const MFInputSocket *> outputs;
  outputs.append(&output_socket);

  network_builder.to_dot__clipboard();

  const MultiFunction &fn = resources.construct<MF_EvaluateNetwork>(
      "expression function", inputs, outputs);

  return fn;
}

}  // namespace Expr
}  // namespace FN
