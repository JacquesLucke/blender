#include "FN_expression_parser.h"

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

static AstNode *parse_expression(TokensToAstBuilder &builder);
static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__power_level(TokensToAstBuilder &builder);
static AstNode *parse_expression__atom_level(TokensToAstBuilder &builder);

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
      builder.consume(TokenType::Minus);
      AstNode *expr = parse_expression__mul_div_level(builder);
      return builder.construct_unary_node(AstNodeType::Negate, expr);
    }
    case TokenType::ParenOpen: {
      builder.consume(TokenType::ParenOpen);
      AstNode *expr = parse_expression(builder);
      builder.consume(TokenType::ParenClose);
      return expr;
    }
    default:
      BLI_assert(false);
      return nullptr;
  }
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

static AstNode *parse_expression__mul_div_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__atom_level(builder);
  TokenType op_token;
  while (ELEM(op_token = builder.next_type(), TokenType::Asterix, TokenType::ForwardSlash)) {
    builder.consume();
    AstNode *right_expr = parse_expression__atom_level(builder);
    AstNodeType node_type = (builder.next_type() == TokenType::Asterix) ? AstNodeType::Multiply :
                                                                          AstNodeType::Divide;
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNode *parse_expression__add_sub_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__mul_div_level(builder);
  TokenType op_token;
  while (ELEM(op_token = builder.next_type(), TokenType::Plus, TokenType::Minus)) {
    builder.consume();
    AstNode *right_expr = parse_expression__mul_div_level(builder);
    AstNodeType node_type = (builder.next_type() == TokenType::Plus) ? AstNodeType::Plus :
                                                                       AstNodeType::Minus;
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static AstNodeType get_comparison_node_type(TokenType token_type)
{
  switch (token_type) {
    case TokenType::Equal:
      return AstNodeType::Equal;
    case TokenType::Less:
      return AstNodeType::Less;
    case TokenType::LessOrEqual:
      return AstNodeType::LessOrEqual;
    case TokenType::Greater:
      return AstNodeType::Greater;
    case TokenType::GreaterOrEqual:
      return AstNodeType::GreaterOrEqual;
    default:
      BLI_assert(false);
      return AstNodeType::Equal;
  }
}

static AstNode *parse_expression__comparison_level(TokensToAstBuilder &builder)
{
  AstNode *left_expr = parse_expression__add_sub_level(builder);
  if (ELEM(builder.next_type(),
           TokenType::Equal,
           TokenType::Less,
           TokenType::LessOrEqual,
           TokenType::Greater,
           TokenType::GreaterOrEqual)) {
    TokenType op_token = builder.next_type();
    builder.consume();
    AstNode *right_expr = parse_expression__add_sub_level(builder);
    AstNodeType node_type = get_comparison_node_type(op_token);
    return builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  else {
    return left_expr;
  }
}

static AstNode *parse_expression(TokensToAstBuilder &builder)
{
  return parse_expression__comparison_level(builder);
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
  BLI_assert(builder.is_at_end());
  AstNode &node = *parse_expression(builder);
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
    case AstNodeType::DoubleLess:
      return "DoubleLess";
    case AstNodeType::DoubleRight:
      return "DoubleRight";
    case AstNodeType::Negate:
      return "Negate";
    case AstNodeType::Power:
      return "Power";
  }
  BLI_assert(false);
  return "";
}

}  // namespace Expr
}  // namespace FN
