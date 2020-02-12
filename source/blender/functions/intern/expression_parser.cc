#include "FN_expression_parser.h"
#include "BLI_dot_export.h"

namespace FN {
namespace Expr {

class TokensToAstBuilder {
 private:
  StringRef m_str;
  ArrayRef<TokenType> m_token_types;
  ArrayRef<TokenRange> m_token_ranges;
  LinearAllocator<> &m_allocator;
  uint m_current = 0;

 public:
  TokensToAstBuilder(StringRef str,
                     ArrayRef<TokenType> token_types,
                     ArrayRef<TokenRange> token_ranges,
                     LinearAllocator<> &allocator)
      : m_str(str),
        m_token_types(token_types),
        m_token_ranges(token_ranges),
        m_allocator(allocator)
  {
    BLI_assert(token_types.last() == TokenType::EndOfString);
  }

  TokenType next_type() const
  {
    return m_token_types[m_current];
  }

  StringRef next_str() const
  {
    BLI_assert(!this->is_at_end());
    TokenRange range = m_token_ranges[m_current];
    return m_str.substr(range.start, range.size);
  }

  StringRef consume_next_str()
  {
    StringRef str = this->next_str();
    m_current++;
    return str;
  }

  bool is_at_end() const
  {
    return m_current == m_token_ranges.size();
  }

  void consume(TokenType token_type)
  {
    UNUSED_VARS_NDEBUG(token_type);
    BLI_assert(this->next_type() == token_type);
    this->consume();
  }

  void consume()
  {
    BLI_assert(!this->is_at_end());
    m_current++;
  }

  IdentifierNode *consume_identifier()
  {
    StringRef token_str = this->consume_next_str();
    StringRefNull identifier = m_allocator.copy_string(token_str);
    return m_allocator.construct<IdentifierNode>(identifier);
  }

  ConstantIntNode *consume_constant_int()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.copy_to__with_null(str);
    int value = std::atoi(str);
    return m_allocator.construct<ConstantIntNode>(value);
  }

  ConstantFloatNode *consume_constant_float()
  {
    StringRef token_str = this->consume_next_str();
    char *str = (char *)alloca(token_str.size() + 1);
    token_str.copy_to__with_null(str);
    float value = std::atof(str);
    return m_allocator.construct<ConstantFloatNode>(value);
  }

  ConstantStringNode *consume_constant_string()
  {
    StringRef token_str = this->consume_next_str();
    StringRefNull value = m_allocator.copy_string(token_str);
    return m_allocator.construct<ConstantStringNode>(value);
  }

  AstNode *construct_binary_node(AstNodeType node_type, AstNode *left_node, AstNode *right_node)
  {
    MutableArrayRef<AstNode *> children = m_allocator.allocate_array<AstNode *>(2);
    children[0] = left_node;
    children[1] = right_node;
    AstNode *node = m_allocator.construct<AstNode>(children, node_type);
    return node;
  }

  AstNode *construct_unary_node(AstNodeType node_type, AstNode *sub_node)
  {
    MutableArrayRef<AstNode *> children = m_allocator.allocate_array<AstNode *>(1);
    children[0] = sub_node;
    AstNode *node = m_allocator.construct<AstNode>(children, node_type);
    return node;
  }
};

static bool is_comparison_token(TokenType token_type)
{
  int type = (int)token_type;
  return type >= (int)TokenType::Less && type <= (int)TokenType::GreaterOrEqual;
}

static AstNodeType get_comparison_node_type(TokenType token_type)
{
  BLI_STATIC_ASSERT((int)TokenType::Less == (int)AstNodeType::Less, "");
  BLI_STATIC_ASSERT((int)TokenType::Greater == (int)AstNodeType::Greater, "");
  BLI_STATIC_ASSERT((int)TokenType::Equal == (int)AstNodeType::Equal, "");
  BLI_STATIC_ASSERT((int)TokenType::LessOrEqual == (int)AstNodeType::LessOrEqual, "");
  BLI_STATIC_ASSERT((int)TokenType::GreaterOrEqual == (int)AstNodeType::GreaterOrEqual, "");
  return (AstNodeType)(int)token_type;
}

static bool is_add_sub_token(TokenType token_type)
{
  return token_type == TokenType::Plus || token_type == TokenType::Minus;
}

static AstNodeType get_add_sub_node_type(TokenType token_type)
{
  BLI_STATIC_ASSERT((int)TokenType::Plus == (int)AstNodeType::Plus, "");
  BLI_STATIC_ASSERT((int)TokenType::Minus == (int)AstNodeType::Minus, "");
  return (AstNodeType)(int)token_type;
}

static bool is_mul_div_token(TokenType token_type)
{
  return token_type == TokenType::Asterix || token_type == TokenType::ForwardSlash;
}

static AstNodeType get_mul_div_node_type(TokenType token_type)
{
  BLI_STATIC_ASSERT((int)TokenType::Asterix == (int)AstNodeType::Multiply, "");
  BLI_STATIC_ASSERT((int)TokenType::ForwardSlash == (int)AstNodeType::Divide, "");
  return (AstNodeType)(int)token_type;
}

static AstNode *parse_expression(TokensToAstBuilder &builder);
static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__power_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__atom_level(TokensToAstBuilder &builder);

static AstNode *parse_expression(TokensToAstBuilder &builder)
{
  return parse_expression__comparison_level(builder);
}

static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__add_sub_level(builder);
  if (is_comparison_token(builder.next_type())) {
    AstNodeType node_type = get_comparison_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__add_sub_level(builder);
    return builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  else {
    return left_expr;
  }
}

static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__mul_div_level(builder);
  while (is_add_sub_token(builder.next_type())) {
    AstNodeType node_type = get_add_sub_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__mul_div_level(builder);
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__atom_level(builder);
  while (is_mul_div_token(builder.next_type())) {
    AstNodeType node_type = get_mul_div_node_type(builder.next_type());
    builder.consume();
    AstNode *right_expr = parse_expression__atom_level(builder);
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNode *parse_expression__power_level(TokensToAstBuilder &builder)
{
  AstNode *base_expr = parse_expression__atom_level(builder);
  if (builder.next_type() == TokenType::DoubleAsterix) {
    builder.consume();
    AstNode *exponent_expr = parse_expression__power_level(builder);
    return builder.construct_binary_node(AstNodeType::Power, base_expr, exponent_expr);
  }
  else {
    return base_expr;
  }
}

static AstNode *parse_expression__atom_level(TokensToAstBuilder &builder)
{
  switch (builder.next_type()) {
    case TokenType::Identifier:
      return builder.consume_identifier();
    case TokenType::IntLiteral:
      return builder.consume_constant_int();
    case TokenType::FloatLiteral:
      return builder.consume_constant_float();
    case TokenType::Minus: {
      builder.consume();
      AstNode *expr = parse_expression__mul_div_level(builder);
      return builder.construct_unary_node(AstNodeType::Negate, expr);
    }
    case TokenType::Plus: {
      builder.consume();
      return parse_expression__mul_div_level(builder);
    }
    case TokenType::ParenOpen: {
      builder.consume();
      AstNode *expr = parse_expression(builder);
      builder.consume(TokenType::ParenClose);
      return expr;
    }
    default:
      BLI_assert(false);
      return nullptr;
  }
}

AstNode &parse_expression(StringRef str, LinearAllocator<> &allocator)
{
  Vector<TokenType> token_types;
  Vector<TokenRange> token_ranges;
  tokenize(str, token_types, token_ranges);

  for (uint i : token_types.index_range()) {
    std::cout << i << ": " << token_type_to_string(token_types[i]) << ": "
              << str.substr(token_ranges[i].start, token_ranges[i].size) << '\n';
  }

  token_types.append(TokenType::EndOfString);
  TokensToAstBuilder builder(str, token_types, token_ranges, allocator);
  AstNode &node = *parse_expression(builder);
  BLI_assert(builder.is_at_end());
  return node;
}

StringRefNull node_type_to_string(AstNodeType node_type)
{
  switch (node_type) {
    case AstNodeType::Identifier:
      return "Identifier";
    case AstNodeType::ConstantInt:
      return "ConstantInt";
    case AstNodeType::ConstantFloat:
      return "ConstantFloat";
    case AstNodeType::ConstantString:
      return "ConstantString";
    case AstNodeType::Plus:
      return "Plus";
    case AstNodeType::Minus:
      return "Minus";
    case AstNodeType::Multiply:
      return "Multiply";
    case AstNodeType::Divide:
      return "Divide";
    case AstNodeType::Less:
      return "Less";
    case AstNodeType::Greater:
      return "Greater";
    case AstNodeType::Equal:
      return "Equal";
    case AstNodeType::LessOrEqual:
      return "LessOrEqual";
    case AstNodeType::GreaterOrEqual:
      return "GreaterOrEqual";
    case AstNodeType::Negate:
      return "Negate";
    case AstNodeType::Power:
      return "Power";
  }
  BLI_assert(false);
  return "";
}

static BLI::DotExport::Node &ast_to_dot_node(BLI::DotExport::DirectedGraph &digraph,
                                             const AstNode &ast_node)
{
  switch (ast_node.type) {
    case AstNodeType::Identifier:
      return digraph.new_node(((IdentifierNode &)ast_node).value);
    case AstNodeType::ConstantFloat:
      return digraph.new_node(std::to_string(((ConstantFloatNode &)ast_node).value));
    case AstNodeType::ConstantInt:
      return digraph.new_node(std::to_string(((ConstantIntNode &)ast_node).value));
    case AstNodeType::ConstantString:
      return digraph.new_node(((ConstantStringNode &)ast_node).value);
    default: {
      BLI::DotExport::Node &root_node = digraph.new_node(node_type_to_string(ast_node.type));
      for (uint i : ast_node.children.index_range()) {
        AstNode &child = *ast_node.children[i];
        BLI::DotExport::Node &dot_child = ast_to_dot_node(digraph, child);
        BLI::DotExport::DirectedEdge &dot_edge = digraph.new_edge(root_node, dot_child);
        dot_edge.set_attribute("label", std::to_string(i));
      }
      return root_node;
    }
  }
}

std::string AstNode::to_dot() const
{
  BLI::DotExport::DirectedGraph digraph;
  ast_to_dot_node(digraph, *this);
  return digraph.to_dot_string();
}

}  // namespace Expr
}  // namespace FN
