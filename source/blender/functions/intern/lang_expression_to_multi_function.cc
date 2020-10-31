/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "FN_lang_multi_function.hh"
#include "FN_lang_parse.hh"
#include "FN_multi_function_network_evaluation.hh"

namespace blender::fn::lang {

class AstToNetworkBuilder {
 private:
  MFNetwork &network_;
  ResourceCollector &resources_;
  const Map<std::string, MFOutputSocket *> &expression_inputs_;
  const MFSymbolTable &symbols_;

 public:
  AstToNetworkBuilder(MFNetwork &network_builder,
                      ResourceCollector &resources,
                      const Map<std::string, MFOutputSocket *> &expression_inputs,
                      const MFSymbolTable &symbols)
      : network_(network_builder),
        resources_(resources),
        expression_inputs_(expression_inputs),
        symbols_(symbols)
  {
  }

  MFOutputSocket &build(AstNode &ast_node)
  {
    switch (ast_node.type) {
      case AstNodeType::Error:
        throw std::runtime_error("There is an error ast node.");
      case AstNodeType::IsLess:
        return this->insert_binary_function("a<b", ast_node);
      case AstNodeType::IsGreater:
        return this->insert_binary_function("a>b", ast_node);
      case AstNodeType::IsEqual:
        return this->insert_binary_function("a==b", ast_node);
      case AstNodeType::IsLessOrEqual:
        return this->insert_binary_function("a<=b", ast_node);
      case AstNodeType::IsGreaterOrEqual:
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
    throw std::runtime_error("Unknown ast node type.");
  }

  template<typename T> MFOutputSocket &insert_constant_function(const T &value)
  {
    const MultiFunction &fn = resources_.construct<CustomMF_Constant<T>>(AT, value);
    MFFunctionNode &node = network_.add_function(fn);
    return node.output(0);
  }

  MFOutputSocket &insert_unary_function(StringRef name, AstNode &ast_node)
  {
    MFOutputSocket *sub = &this->build(*ast_node.children[0]);
    return this->insert_function(name, {sub});
  }

  MFOutputSocket &insert_binary_function(StringRef name, AstNode &ast_node)
  {
    MFOutputSocket *sub1 = &this->build(*ast_node.children[0]);
    MFOutputSocket *sub2 = &this->build(*ast_node.children[1]);
    return this->insert_function(name, {sub1, sub2});
  }

  MFOutputSocket &insert_identifier(AstNode &ast_node)
  {
    IdentifierNode &identifier_node = (IdentifierNode &)ast_node;
    StringRef identifier = identifier_node.value;
    MFOutputSocket *expression_input_socket = expression_inputs_.lookup_default(identifier,
                                                                                nullptr);
    if (expression_input_socket != nullptr) {
      return *expression_input_socket;
    }
    const GMutablePointer *constant = symbols_.try_lookup_single_constant(identifier);
    if (constant == nullptr) {
      throw std::runtime_error("Unknown identifier: " + identifier);
    }

    const MultiFunction &fn = resources_.construct<CustomMF_GenericConstant>(
        AT, *constant->type(), constant->get());
    return network_.add_function(fn).output(0);
  }

  MFOutputSocket &insert_call(CallNode &call_node)
  {
    Vector<MFOutputSocket *> arg_sockets;
    for (AstNode *child : call_node.children) {
      arg_sockets.append(&this->build(*child));
    }
    return this->insert_function(call_node.name, arg_sockets);
  }

  MFOutputSocket &insert_attribute(AttributeNode &attribute_node)
  {
    MFOutputSocket &sub = this->build(*attribute_node.children[0]);
    MFDataType type = sub.data_type();
    const MultiFunction *fn = symbols_.try_lookup_attribute(type, attribute_node.name);
    if (fn == nullptr) {
      throw std::runtime_error(type.to_string() + " has no attribute " + attribute_node.name);
    }

    MFNode &node = network_.add_function(*fn);
    network_.add_link(sub, node.input(0));
    return node.output(0);
  }

  MFOutputSocket &insert_method_call(MethodCallNode &method_call_node)
  {
    Vector<MFOutputSocket *> arg_sockets;
    for (AstNode *child : method_call_node.children) {
      arg_sockets.append(&this->build(*child));
    }
    MFDataType type = arg_sockets[0]->data_type();
    const MultiFunction *fn = symbols_.try_lookup_method(type, method_call_node.name);
    if (fn == nullptr) {
      throw std::runtime_error(type.to_string() + " has no method " + method_call_node.name);
    }

    MFNode &node = network_.add_function(*fn);
    if (arg_sockets.size() != node.inputs().size()) {
      throw std::runtime_error("Passed wrong number of parameters to " + fn->name());
    }

    for (int i : arg_sockets.index_range()) {
      this->insert_link_with_conversion(*arg_sockets[i], node.input(i));
    }
    return node.output(0);
  }

  void insert_link_with_conversion(MFOutputSocket &from, MFInputSocket &to)
  {
    MFOutputSocket &new_from = this->maybe_insert_conversion(from, to.data_type());
    network_.add_link(new_from, to);
  }

  MFOutputSocket &maybe_insert_conversion(MFOutputSocket &socket, MFDataType target_type)
  {
    MFDataType from_type = socket.data_type();
    if (from_type == target_type) {
      return socket;
    }
    const MultiFunction *conversion_fn = symbols_.try_lookup_conversion(from_type, target_type);
    if (conversion_fn == nullptr) {
      throw std::runtime_error("Cannot convert from " + from_type.to_string() + " to " +
                               target_type.to_string());
    }

    MFNode &conversion_node = network_.add_function(*conversion_fn);
    network_.add_link(socket, conversion_node.input(0));
    return conversion_node.output(0);
  }

  MFOutputSocket &insert_function(StringRef name, Span<MFOutputSocket *> arg_sockets)
  {
    Vector<MFDataType> arg_types;
    for (MFOutputSocket *socket : arg_sockets) {
      arg_types.append(socket->data_type());
    }
    const MultiFunction *fn = this->lookup_function(name, arg_types);
    if (fn == nullptr) {
      throw std::runtime_error("Function " + name + " does not exist for these parameters.");
    }
    MFNode &node = network_.add_function(*fn);
    if (arg_sockets.size() != node.inputs().size()) {
      throw std::runtime_error("Passed wrong number of parameters to " + fn->name());
    }
    for (int i : arg_sockets.index_range()) {
      this->insert_link_with_conversion(*arg_sockets[i], node.input(i));
    }
    return node.output(0);
  }

  const MultiFunction *lookup_function(StringRef name, Span<MFDataType> arg_types)
  {
    Span<const MultiFunction *> candidates = symbols_.lookup_function_candidates(name);
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
    return best_fit_yet;
  }

  /* Return -1, when the function cannot be used. Otherwise, lower return values mean a better fit.
   */
  int get_function_suitability(const MultiFunction &fn, Span<MFDataType> arg_types)
  {
    int input_index = 0;
    int conversion_count = 0;
    for (int param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      if (param_type.is_input_or_mutable()) {
        if (input_index >= arg_types.size()) {
          /* Number of arguments does not match. */
          return -1;
        }

        MFDataType actual_type = arg_types[input_index];
        MFDataType expected_type = param_type.data_type();
        if (actual_type != expected_type) {
          if (symbols_.can_convert(actual_type, expected_type)) {
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

static MFOutputSocket &expression_to_network(StringRef str,
                                             MFDataType output_type,
                                             ResourceCollector &resources,
                                             Map<std::string, MFOutputSocket *> &expression_inputs,
                                             const MFSymbolTable &symbols,
                                             MFNetwork &network_builder)
{
  LinearAllocator<> ast_allocator;
  AstNode &ast_node = parse_expression(str, ast_allocator);

  AstToNetworkBuilder builder{network_builder, resources, expression_inputs, symbols};
  MFOutputSocket &output_socket = builder.build(ast_node);
  return builder.maybe_insert_conversion(output_socket, output_type);
}

const MultiFunction &expression_to_multi_function(StringRef expression,
                                                  const MFSymbolTable &symbols,
                                                  ResourceCollector &resources,
                                                  MFDataType return_type,
                                                  Span<MFDataTypeWithName> parameters)
{
  MFNetwork &network = resources.construct<MFNetwork>(__func__);
  Map<std::string, MFOutputSocket *> expression_inputs;
  Vector<const MFOutputSocket *> inputs;
  for (const MFDataTypeWithName &parameter : parameters) {
    MFOutputSocket &socket = network.add_input(parameter.name, parameter.data_type);
    inputs.append(&socket);
    expression_inputs.add_new(parameter.name, &socket);
  }

  MFOutputSocket &expr_output = expression_to_network(
      expression, return_type, resources, expression_inputs, symbols, network);
  MFInputSocket &builder_output = network.add_output("Result", return_type);
  network.add_link(expr_output, builder_output);

  Vector<const MFInputSocket *> outputs;
  outputs.append(&builder_output);

  const MultiFunction &fn = resources.construct<MFNetworkEvaluator>(
      "expression function", inputs, outputs);

  return fn;
}

}  // namespace blender::fn::lang
