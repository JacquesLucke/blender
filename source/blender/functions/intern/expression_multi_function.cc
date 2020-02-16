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
  const SymbolTable &m_symbols;

 public:
  AstToNetworkBuilder(MFNetworkBuilder &network_builder,
                      ResourceCollector &resources,
                      const StringMap<MFBuilderOutputSocket *> &expression_inputs,
                      const SymbolTable &symbols)
      : m_network_builder(network_builder),
        m_resources(resources),
        m_expression_inputs(expression_inputs),
        m_symbols(symbols)
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
      case AstNodeType::Power:
        return this->insert_binary_function("a**b", ast_node);
      case AstNodeType::Negate:
        return this->insert_unary_function("-a", ast_node);
      case AstNodeType::ConstantInt:
        return this->insert_constant_function<int>(((ConstantIntNode &)ast_node).value);
      case AstNodeType::ConstantFloat:
        return this->insert_constant_function<float>(((ConstantFloatNode &)ast_node).value);
      case AstNodeType::ConstantString:
        return this->insert_constant_function<std::string>(((ConstantStringNode &)ast_node).value);
      case AstNodeType::Call:
        return this->insert_call((CallNode &)ast_node);
      case AstNodeType::Identifier:
        return this->insert_identifier(ast_node);
      case AstNodeType::Attribute:
        return this->insert_attribute((AttributeNode &)ast_node);
      case AstNodeType::MethodCall:
        return this->insert_method_call((MethodCallNode &)ast_node);
    }
    BLI_assert(false);
    return this->build(ast_node);
  }

  template<typename T> MFBuilderOutputSocket &insert_constant_function(const T &value)
  {
    MFBuilderFunctionNode &node = m_network_builder.add_function<MF_ConstantValue<T>>(m_resources,
                                                                                      value);
    return node.output(0);
  }

  MFBuilderOutputSocket &insert_unary_function(StringRef name, AstNode &ast_node)
  {
    MFBuilderOutputSocket *sub = &this->build(*ast_node.children[0]);
    return this->insert_function(name, {sub});
  }

  MFBuilderOutputSocket &insert_binary_function(StringRef name, AstNode &ast_node)
  {
    MFBuilderOutputSocket *sub1 = &this->build(*ast_node.children[0]);
    MFBuilderOutputSocket *sub2 = &this->build(*ast_node.children[1]);
    return this->insert_function(name, {sub1, sub2});
  }

  MFBuilderOutputSocket &insert_identifier(AstNode &ast_node)
  {
    IdentifierNode &identifier_node = (IdentifierNode &)ast_node;
    StringRef identifier = identifier_node.value;
    MFBuilderOutputSocket *expression_input_socket = m_expression_inputs.lookup_default(identifier,
                                                                                        nullptr);
    if (expression_input_socket != nullptr) {
      return *expression_input_socket;
    }
    Optional<SingleConstant> constant = m_symbols.try_lookup_single_constant(identifier);
    BLI_assert(constant.has_value());
    return m_network_builder
        .add_function<MF_GenericConstantValue>(m_resources, *constant->type, constant->buffer)
        .output(0);
  }

  MFBuilderOutputSocket &insert_call(CallNode &call_node)
  {
    Vector<MFBuilderOutputSocket *> arg_sockets;
    for (AstNode *child : call_node.children) {
      arg_sockets.append(&this->build(*child));
    }
    return this->insert_function(call_node.name, arg_sockets);
  }

  MFBuilderOutputSocket &insert_attribute(AttributeNode &attribute_node)
  {
    MFBuilderOutputSocket &sub = this->build(*attribute_node.children[0]);
    MFDataType type = sub.data_type();
    const MultiFunction *fn = m_symbols.try_lookup_attribute(type, attribute_node.name);
    BLI_assert(fn != nullptr);

    MFBuilderNode &node = m_network_builder.add_function(*fn);
    m_network_builder.add_link(sub, node.input(0));
    return node.output(0);
  }

  MFBuilderOutputSocket &insert_method_call(MethodCallNode &method_call_node)
  {
    Vector<MFBuilderOutputSocket *> arg_sockets;
    for (AstNode *child : method_call_node.children) {
      arg_sockets.append(&this->build(*child));
    }
    MFDataType type = arg_sockets[0]->data_type();
    const MultiFunction *fn = m_symbols.try_lookup_method(type, method_call_node.name);
    BLI_assert(fn != nullptr);

    MFBuilderNode &node = m_network_builder.add_function(*fn);
    BLI::assert_same_size(arg_sockets, node.inputs());

    for (uint i : arg_sockets.index_range()) {
      this->insert_link_with_conversion(*arg_sockets[i], node.input(i));
    }
    return node.output(0);
  }

  void insert_link_with_conversion(MFBuilderOutputSocket &from, MFBuilderInputSocket &to)
  {
    MFBuilderOutputSocket &new_from = this->maybe_insert_conversion(from, to.data_type());
    m_network_builder.add_link(new_from, to);
  }

  MFBuilderOutputSocket &maybe_insert_conversion(MFBuilderOutputSocket &socket,
                                                 MFDataType target_type)
  {
    MFDataType from_type = socket.data_type();
    if (from_type == target_type) {
      return socket;
    }
    const MultiFunction *conversion_fn = m_symbols.try_lookup_conversion(from_type, target_type);
    BLI_assert(conversion_fn != nullptr);
    MFBuilderNode &conversion_node = m_network_builder.add_function(*conversion_fn);
    m_network_builder.add_link(socket, conversion_node.input(0));
    return conversion_node.output(0);
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
      this->insert_link_with_conversion(*arg_sockets[i], node.input(i));
    }
    return node.output(0);
  }

  const MultiFunction &lookup_function(StringRef name, ArrayRef<MFDataType> arg_types)
  {
    ArrayRef<const MultiFunction *> candidates = m_symbols.lookup_function_candidates(name);
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
          if (m_symbols.can_convert(actual_type, expected_type)) {
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

static MFBuilderOutputSocket &expression_to_network(
    StringRef str,
    MFDataType output_type,
    ResourceCollector &resources,
    StringMap<MFBuilderOutputSocket *> &expression_inputs,
    const SymbolTable &symbols,
    MFNetworkBuilder &network_builder)
{
  LinearAllocator<> ast_allocator;
  AstNode &ast_node = parse_expression(str, ast_allocator);

  AstToNetworkBuilder builder{network_builder, resources, expression_inputs, symbols};
  MFBuilderOutputSocket &output_socket = builder.build(ast_node);
  return builder.maybe_insert_conversion(output_socket, output_type);
}

const MultiFunction &expression_to_multi_function(StringRef str,
                                                  MFDataType output_type,
                                                  ResourceCollector &resources,
                                                  ArrayRef<StringRef> variable_names,
                                                  ArrayRef<MFDataType> variable_types,
                                                  const SymbolTable &symbols)
{
  BLI::assert_same_size(variable_names, variable_types);

  MFNetworkBuilder network_builder;
  StringMap<MFBuilderOutputSocket *> expression_inputs;
  for (uint i : variable_names.index_range()) {
    StringRef identifier = variable_names[i];
    MFBuilderDummyNode &node = network_builder.add_dummy(
        identifier, {}, {variable_types[i]}, {}, {"Value"});
    expression_inputs.add_new(identifier, &node.output(0));
  }

  MFBuilderOutputSocket &expr_builder_output = expression_to_network(
      str, output_type, resources, expression_inputs, symbols, network_builder);
  MFBuilderDummyNode &builder_output = network_builder.add_dummy(
      "Result", {output_type}, {}, {"Value"}, {});
  network_builder.add_link(expr_builder_output, builder_output.input(0));

  MFNetwork &network = resources.construct<MFNetwork>("expression network", network_builder);
  const MFInputSocket &output_socket = network.find_dummy_socket(builder_output.input(0));

  Vector<const MFOutputSocket *> inputs;
  expression_inputs.foreach_value([&](MFBuilderOutputSocket *builder_input) {
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
