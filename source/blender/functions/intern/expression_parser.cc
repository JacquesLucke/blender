#include "FN_expression_parser.h"

namespace FN {
namespace Expr {

class TokensToAstBuilder {
 private:
  StringRef m_str;
  ArrayRef<TokenType::Enum> m_token_types;
  ArrayRef<TokenRange> m_token_ranges;
  MonotonicAllocator<> &m_allocator;
  uint m_current = 0;

 public:
  TokensToAstBuilder(StringRef str,
                     ArrayRef<TokenType::Enum> token_types,
                     ArrayRef<TokenRange> token_ranges,
                     MonotonicAllocator<> &allocator)
      : m_str(str),
        m_token_types(token_types),
        m_token_ranges(token_ranges),
        m_allocator(allocator)
  {
    BLI_assert(token_types.last() == TokenType::EndOfString);
  }

  TokenType::Enum next_type() const
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

  void consume(TokenType::Enum token_type)
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

  ASTNode *construct_binary_node(ASTNodeType::Enum node_type,
                                 ASTNode *left_node,
                                 ASTNode *right_node)
  {
    MutableArrayRef<ASTNode *> children = m_allocator.allocate_array<ASTNode *>(2);
    children[0] = left_node;
    children[1] = right_node;
    ASTNode *node = m_allocator.construct<ASTNode>(children, node_type);
    return node;
  }

  ASTNode *construct_unary_node(ASTNodeType::Enum node_type, ASTNode *sub_node)
  {
    MutableArrayRef<ASTNode *> children = m_allocator.allocate_array<ASTNode *>(1);
    children[0] = sub_node;
    ASTNode *node = m_allocator.construct<ASTNode>(children, node_type);
    return node;
  }
};

static ASTNode *parse_expression(TokensToAstBuilder &builder);
static ASTNode *parse_expression__comparison_level(TokensToAstBuilder &builder);
static ASTNode *parse_expression__add_sub_level(TokensToAstBuilder &builder);
static ASTNode *parse_expression__mul_div_level(TokensToAstBuilder &builder);
static ASTNode *parse_expression__power_level(TokensToAstBuilder &builder);
static ASTNode *parse_expression__atom_level(TokensToAstBuilder &builder);

static ASTNode *parse_expression__atom_level(TokensToAstBuilder &builder)
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
      ASTNode *expr = parse_expression__mul_div_level(builder);
      return builder.construct_unary_node(ASTNodeType::Negate, expr);
    }
    case TokenType::ParenOpen: {
      builder.consume(TokenType::ParenOpen);
      ASTNode *expr = parse_expression(builder);
      builder.consume(TokenType::ParenClose);
      return expr;
    }
    default:
      BLI_assert(false);
      return nullptr;
  }
}

static ASTNode *parse_expression__power_level(TokensToAstBuilder &builder)
{
  ASTNode *base_expr = parse_expression__atom_level(builder);
  if (builder.next_type() == TokenType::DoubleAsterix) {
    builder.consume();
    ASTNode *exponent_expr = parse_expression__power_level(builder);
    return builder.construct_binary_node(ASTNodeType::Power, base_expr, exponent_expr);
  }
  else {
    return base_expr;
  }
}

static ASTNode *parse_expression__mul_div_level(TokensToAstBuilder &builder)
{
  ASTNode *left_expr = parse_expression__atom_level(builder);
  TokenType::Enum op_token;
  while (ELEM(op_token = builder.next_type(), TokenType::Asterix, TokenType::ForwardSlash)) {
    builder.consume();
    ASTNode *right_expr = parse_expression__atom_level(builder);
    ASTNodeType::Enum node_type = (builder.next_type() == TokenType::Asterix) ?
                                      ASTNodeType::Multiply :
                                      ASTNodeType::Divide;
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static ASTNode *parse_expression__add_sub_level(TokensToAstBuilder &builder)
{
  ASTNode *left_expr = parse_expression__mul_div_level(builder);
  TokenType::Enum op_token;
  while (ELEM(op_token = builder.next_type(), TokenType::Plus, TokenType::Minus)) {
    builder.consume();
    ASTNode *right_expr = parse_expression__mul_div_level(builder);
    ASTNodeType::Enum node_type = (builder.next_type() == TokenType::Plus) ? ASTNodeType::Plus :
                                                                             ASTNodeType::Minus;
    left_expr = builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  return left_expr;
}

static ASTNodeType::Enum get_comparison_node_type(TokenType::Enum token_type)
{
  switch (token_type) {
    case TokenType::Equal:
      return ASTNodeType::Equal;
    case TokenType::Less:
      return ASTNodeType::Less;
    case TokenType::LessOrEqual:
      return ASTNodeType::LessOrEqual;
    case TokenType::Greater:
      return ASTNodeType::Greater;
    case TokenType::GreaterOrEqual:
      return ASTNodeType::GreaterOrEqual;
    default:
      BLI_assert(false);
      return ASTNodeType::Equal;
  }
}

static ASTNode *parse_expression__comparison_level(TokensToAstBuilder &builder)
{
  ASTNode *left_expr = parse_expression__add_sub_level(builder);
  if (ELEM(builder.next_type(),
           TokenType::Equal,
           TokenType::Less,
           TokenType::LessOrEqual,
           TokenType::Greater,
           TokenType::GreaterOrEqual)) {
    builder.consume();
    ASTNode *right_expr = parse_expression__add_sub_level(builder);
    ASTNodeType::Enum node_type = get_comparison_node_type(builder.next_type());
    return builder.construct_binary_node(node_type, left_expr, right_expr);
  }
  else {
    return left_expr;
  }
}

static ASTNode *parse_expression(TokensToAstBuilder &builder)
{
  return parse_expression__comparison_level(builder);
}

ASTNode &parse_expression(StringRef str, MonotonicAllocator<> &allocator)
{
  Vector<TokenType::Enum> token_types;
  Vector<TokenRange> token_ranges;
  tokenize(str, token_types, token_ranges);

  token_types.append(TokenType::EndOfString);
  TokensToAstBuilder builder(str, token_types, token_ranges, allocator);
  BLI_assert(builder.is_at_end());
  ASTNode &node = *parse_expression(builder);
  return node;
}

}  // namespace Expr
}  // namespace FN
