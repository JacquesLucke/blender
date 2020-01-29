#ifndef __FN_MULTI_FUNCTION_EXPRESSION_H__
#define __FN_MULTI_FUNCTION_EXPRESSION_H__

#include "FN_multi_function_network.h"
#include "BLI_string_map.h"

namespace FN {

using BLI::StringMap;

namespace ExprNodeType {
enum Enum { Variable, Function };
}

class ExprNode {
 private:
  ExprNodeType::Enum m_type;
  MFDataType m_output_type;

  friend class VariableExprNode;
  friend class FunctionExprNode;

  ExprNode(ExprNodeType::Enum type, MFDataType output_type)
      : m_type(type), m_output_type(output_type)
  {
  }

 public:
  MFDataType output_type() const
  {
    return m_output_type;
  }

  virtual MFBuilderOutputSocket &build_network(
      MFNetworkBuilder &network_builder,
      const StringMap<MFBuilderOutputSocket *> &variable_inputs) const = 0;
};

class VariableExprNode : public ExprNode {
 private:
  std::string m_name;

 public:
  VariableExprNode(std::string name, MFDataType data_type)
      : ExprNode(ExprNodeType::Variable, data_type), m_name(std::move(name))
  {
  }

  MFBuilderOutputSocket &build_network(
      MFNetworkBuilder &UNUSED(network_builder),
      const StringMap<MFBuilderOutputSocket *> &variable_inputs) const override
  {
    MFBuilderOutputSocket *socket = variable_inputs.lookup(m_name);
    return *socket;
  }
};

class FunctionExprNode : public ExprNode {
 private:
  const MultiFunction &m_fn;
  uint m_output_param;
  Vector<const ExprNode *> m_inputs;

 public:
  FunctionExprNode(const MultiFunction &fn, uint output_param, Vector<const ExprNode *> inputs)
      : ExprNode(ExprNodeType::Function, fn.param_type(output_param).data_type()),
        m_fn(fn),
        m_output_param(output_param),
        m_inputs(std::move(inputs))
  {
    BLI_assert(fn.param_type(output_param).is_output_or_mutable());

    uint input_index = 0;
    for (uint param_index : fn.param_indices()) {
      MFParamType param_type = fn.param_type(param_index);
      if (param_type.is_input_or_mutable()) {
        BLI_assert(param_type.data_type() == m_inputs[input_index]->output_type());
        input_index++;
      }
    }
  }

  MFBuilderOutputSocket &build_network(
      MFNetworkBuilder &network_builder,
      const StringMap<MFBuilderOutputSocket *> &variable_inputs) const override
  {
    MFBuilderFunctionNode &node = network_builder.add_function(m_fn);
    for (uint input_index : m_inputs.index_range()) {
      const ExprNode *expr_node = m_inputs[input_index];
      MFBuilderOutputSocket &origin = expr_node->build_network(network_builder, variable_inputs);
      network_builder.add_link(origin, node.input(input_index));
    }

    return node.output_for_param(m_output_param);
  }
};

}  // namespace FN

#endif /* __FN_MULTI_FUNCTION_EXPRESSION_H__ */
