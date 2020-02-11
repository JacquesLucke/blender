#include "FN_expression_multi_function.h"
#include "FN_expression_parser.h"
#include "FN_multi_function_network.h"
#include "FN_multi_functions.h"

namespace FN {
namespace Expr {

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
      BLI_assert(false);
      break;
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
      BLI_assert(false);
      break;
    }
    case AstNodeType::ConstantFloat: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::ConstantString: {
      BLI_assert(false);
      break;
    }
    case AstNodeType::Negate: {
      BLI_assert(false);
      break;
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
