#include "FN_expression_multi_function.h"
#include "FN_expression_parser.h"
#include "FN_multi_function_network.h"
#include "FN_multi_functions.h"

namespace FN {
namespace Expr {

static void insert_implicit_conversions(ResourceCollector &resources,
                                        MFBuilderOutputSocket **sub1,
                                        MFBuilderOutputSocket **sub2)
{
  MFNetworkBuilder &network_builder = (*sub1)->node().network();
  const CPPType &type1 = (*sub1)->data_type().single__cpp_type();
  const CPPType &type2 = (*sub2)->data_type().single__cpp_type();
  if (type1 == type2) {
  }
  else if (type1 == CPP_TYPE<float>() && type2 == CPP_TYPE<int>()) {
    MFBuilderFunctionNode &node = network_builder.add_function<MF_Convert<int, float>>(resources);
    network_builder.add_link(**sub2, node.input(0));
    *sub2 = &node.output(0);
  }
  else if (type1 == CPP_TYPE<int>() && type2 == CPP_TYPE<float>()) {
    MFBuilderFunctionNode &node = network_builder.add_function<MF_Convert<int, float>>(resources);
    network_builder.add_link(**sub1, node.input(0));
    *sub1 = &node.output(0);
  }
  else {
    BLI_assert(false);
  }
}

MFBuilderOutputSocket &build_node(AstNode &ast_node,
                                  MFNetworkBuilder &network_builder,
                                  ResourceCollector &resources)
{
  switch (ast_node.type) {
    case AstNodeType::Less: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Greater: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Equal: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::LessOrEqual: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::GreaterOrEqual: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Plus: {
      MFBuilderOutputSocket *sub1 = &build_node(*ast_node.children[0], network_builder, resources);
      MFBuilderOutputSocket *sub2 = &build_node(*ast_node.children[1], network_builder, resources);
      insert_implicit_conversions(resources, &sub1, &sub2);

      MFBuilderFunctionNode *node;
      const CPPType &type = sub1->data_type().single__cpp_type();
      if (type == CPP_TYPE<int>()) {
        node = &network_builder.add_function<MF_Custom_In2_Out1<int, int, int>>(
            resources, "add", [](int a, int b) { return a + b; });
      }
      else if (type == CPP_TYPE<float>()) {
        node = &network_builder.add_function<MF_Custom_In2_Out1<float, float, float>>(
            resources, "add", [](float a, float b) { return a + b; });
      }
      else {
        BLI_assert(false);
      }

      network_builder.add_link(*sub1, node->input(0));
      network_builder.add_link(*sub2, node->input(1));
      return node->output(0);
    }
    case AstNodeType::Minus: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Multiply: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Divide: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Identifier: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::ConstantInt: {
      ConstantIntNode &int_node = (ConstantIntNode &)ast_node;
      MFBuilderFunctionNode &node = network_builder.add_function<MF_ConstantValue<int>>(
          resources, int_node.value);
      return node.output(0);
    }
    case AstNodeType::ConstantFloat: {
      ConstantFloatNode &float_node = (ConstantFloatNode &)ast_node;
      MFBuilderFunctionNode &node = network_builder.add_function<MF_ConstantValue<float>>(
          resources, float_node.value);
      return node.output(0);
    }
    case AstNodeType::ConstantString: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Negate: {
      MFBuilderOutputSocket &sub_output = build_node(
          *ast_node.children[0], network_builder, resources);
      MFBuilderFunctionNode *node;
      if (sub_output.data_type().single__cpp_type() == CPP_TYPE<int>()) {
        node = &network_builder.add_function<MF_Custom_In1_Out1<int, int>>(
            resources, "negate", [](int a) { return -a; });
      }
      else if (sub_output.data_type().single__cpp_type() == CPP_TYPE<float>()) {
        node = &network_builder.add_function<MF_Custom_In1_Out1<float, float>>(
            resources, "negate", [](float a) { return -a; });
      }
      else {
        BLI_assert(false);
      }
      network_builder.add_link(sub_output, node->input(0));
      return node->output(0);
    }
    case AstNodeType::Power: {
      BLI_assert(false);
      break;
    }
  }
  BLI_assert(false);
  return network_builder.node_by_id(0).output(0);
}

const MultiFunction &expression_to_multi_function(StringRef str, ResourceCollector &resources)
{
  AstNode &ast_node = parse_expression(str, resources.allocator());
  MFNetworkBuilder network_builder;
  MFBuilderOutputSocket &builder_output_socket = build_node(ast_node, network_builder, resources);
  MFBuilderDummyNode &builder_output = network_builder.add_output_dummy("Result",
                                                                        builder_output_socket);

  MFNetwork &network = resources.construct<MFNetwork>("expression network", network_builder);
  const MFInputSocket &output_socket = network.find_dummy_socket(builder_output.input(0));

  Vector<const MFOutputSocket *> inputs;
  Vector<const MFInputSocket *> outputs;
  outputs.append(&output_socket);

  const MultiFunction &fn = resources.construct<MF_EvaluateNetwork>(
      "expression function", inputs, outputs);

  return fn;
}

}  // namespace Expr
}  // namespace FN
